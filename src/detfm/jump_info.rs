use rabc::abc::{
    parser::{Instruction, Op},
    Method,
};
use std::{collections::HashMap, mem::swap};
use thiserror::Error;

#[derive(Error, Debug)]
pub enum JumpInfoError {
    #[error("Invalid number of targets, expected {0} got {1} instead.")]
    InvalidTargets(usize, usize),
    #[error("Invalid instruction with jump targets: {0:?}")]
    InvalidInstructionTargets(Instruction),
    #[error("Cannot last instruction")]
    TraillingInstructions(),
}

#[derive(Debug)]
enum ErrorField {
    From,
    To,
    Target,
}

#[derive(Debug)]
pub struct JumpInfo {
    targets: HashMap<u32, Vec<u32>>,
    jumps_here: HashMap<u32, Vec<u32>>,
    exceptions: HashMap<u32, Vec<(usize, ErrorField)>>,
    /// contains the removed address to graft to the next added instruction
    removed: Vec<u32>,
    /// New instructions
    instructions: Vec<Instruction>,
    modified: bool,
}

impl JumpInfo {
    pub fn new(method: &Method, instructions: &[Instruction]) -> Self {
        let mut targets: HashMap<u32, Vec<u32>> = HashMap::new();
        let mut jumps_here: HashMap<u32, Vec<u32>> = HashMap::new();
        let mut exceptions: HashMap<u32, Vec<(usize, ErrorField)>> = HashMap::new();

        for ins in instructions {
            // if !ins.targets.is_empty() {
            //     println!("Ins: {:?} targets: {:?}", ins.op, ins.targets)
            // }
            for &target in &ins.targets {
                targets.entry(ins.addr).or_default().push(target);
                jumps_here.entry(target).or_default().push(ins.addr);
            }
        }

        for (i, e) in method.exceptions.iter().enumerate() {
            exceptions
                .entry(e.from)
                .or_default()
                .push((i, ErrorField::From));
            exceptions
                .entry(e.to)
                .or_default()
                .push((i, ErrorField::To));
            exceptions
                .entry(e.target)
                .or_default()
                .push((i, ErrorField::Target));
        }
        Self {
            targets,
            jumps_here,
            exceptions,
            removed: Vec::new(),
            instructions: Vec::with_capacity(instructions.len()),
            modified: false,
        }
    }

    pub fn modified(&self) -> bool {
        self.modified
    }

    pub fn add(&mut self, ins: Instruction) {
        if !self.removed.is_empty() {
            // let mut removed = Vec::with_capacity(self.removed.capacity());
            // swap(&mut removed, &mut self.removed);
            let removed: Vec<_> = self.removed.drain(..).collect();
            for addr in removed {
                self.do_remove(addr, ins.addr);
            }
        }
        self.instructions.push(ins);
    }
    pub fn pop(&mut self) {
        if let Some(ins) = self.instructions.pop() {
            self.remove(ins);
        }
    }
    pub fn remove(&mut self, ins: Instruction) {
        self.removed.push(ins.addr);
    }

    fn do_remove(&mut self, addr: u32, next_addr: u32) {
        self.modified = true;
        // println!("jumps_here: {:?} ({addr} -> {next_addr})", self.jumps_here);
        if let Some(jumps_here) = self.jumps_here.remove(&addr) {
            // println!("jumps_here: {jumps_here:?}  {addr} -> {next_addr}");
            for i in &jumps_here {
                let target = self
                    .targets
                    .get_mut(i)
                    .unwrap()
                    .iter_mut()
                    .find(|j| **j == addr)
                    .unwrap();
                *target = next_addr;
            }
            self.jumps_here
                .entry(next_addr)
                .or_default()
                .extend(jumps_here);
            // println!("jumps_here: {:?}", self.jumps_here);
        }

        if let Some(old) = self.exceptions.remove(&addr) {
            let new = self.exceptions.entry(next_addr);
            new.or_default().extend(old);
        }
    }

    pub fn jumps(&self, addr: u32, i: usize) -> u32 {
        self.targets[&addr][i]
    }

    pub fn fix_addresses(
        &mut self,
        method: &mut Method,
    ) -> Result<Vec<Instruction>, JumpInfoError> {
        if !self.removed.is_empty() {
            return Err(JumpInfoError::TraillingInstructions());
        }
        let mut instructions = Vec::new();
        swap(&mut self.instructions, &mut instructions);
        // recompute the instructions address
        let mut pos = 0;
        let mut old2new = HashMap::new();
        let mut new2old = HashMap::new();
        for ins in &mut instructions {
            old2new.insert(ins.addr, pos);
            new2old.insert(pos, ins.addr);
            ins.addr = pos;
            pos += ins.size();
        }
        /* println!(
            "old2new: {}",
            old2new
                .iter()
                .sorted_by_key(|it| it.0)
                .map(|(k, v)| format!("{k}: {v}"))
                .join(", ")
        );
        println!("targets: {:?}", self.targets); */
        // recompute the jumps addresses
        for ins in &mut instructions {
            match &mut ins.op {
                Op::IfNlt(arg)
                | Op::IfNle(arg)
                | Op::IfNgt(arg)
                | Op::IfNge(arg)
                | Op::Jump(arg)
                | Op::IfTrue(arg)
                | Op::IfFalse(arg)
                | Op::IfEq(arg)
                | Op::IfNe(arg)
                | Op::IfLt(arg)
                | Op::IfLe(arg)
                | Op::IfGt(arg)
                | Op::IfGe(arg)
                | Op::IfStrictEq(arg)
                | Op::IfStrictNe(arg) => {
                    if ins.targets.len() != 1 {
                        return Err(JumpInfoError::InvalidTargets(1, ins.targets.len()));
                    }
                    arg.target = old2new[&self.jumps(new2old[&ins.addr], 0)];
                }
                Op::LookupSwitch(arg) => {
                    let expected = arg.targets.len() + 1;
                    if ins.targets.len() != expected {
                        return Err(JumpInfoError::InvalidTargets(expected, ins.targets.len()));
                    }
                    arg.default_target = old2new[&self.jumps(new2old[&ins.addr], 0)];
                    for (i, target) in arg.targets.iter_mut().enumerate() {
                        *target = old2new[&self.jumps(new2old[&ins.addr], i)];
                    }
                }
                _ if ins.targets.is_empty() => {}
                _ => return Err(JumpInfoError::InvalidInstructionTargets(ins.clone())),
            }
        }

        // recompute exceptions targets
        // if !self.exceptions.is_empty() {panic!("hi");}
        for (addr, errs) in &self.exceptions {
            for (i, field) in errs {
                let err = &mut method.exceptions[*i];
                let value = match field {
                    ErrorField::From => &mut err.from,
                    ErrorField::To => &mut err.to,
                    ErrorField::Target => &mut err.target,
                };
                *value = old2new[addr];
            }
        }
        // println!("exceptions: {:?}", method.exceptions);
        Ok(instructions)
    }
}
