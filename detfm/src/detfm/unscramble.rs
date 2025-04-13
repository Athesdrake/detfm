use crate::detfm::classes::StaticClass;

use super::{classes::Classes, jump_info::JumpInfo, Detfm};
use rabc::{
    abc::parser::{
        opargs::{PushDoubleArg, PushStringArg},
        Instruction, Op, OpCode,
    },
    error::RabcError,
};
use thiserror::Error;

#[derive(Error, Debug)]
pub enum UnscrambleError {
    // #[error("Cannot pop value from an empty stack.")]
    // EmptyStack,
    // #[error("Unsupported opcode: {0}")]
    // UnsupportedOpCode(OpCode),
    // #[error("Invalid number of targets, expected {0} got {1} instead.")]
    // InvalidTargets(usize, usize),
    // #[error("Invalid instruction with jump targets: {0:?}")]
    // InvalidInstructionTargets(Instruction),
    #[error(transparent)]
    RabcError(#[from] RabcError),
}

pub fn unscramble_method(
    detfm: &mut Detfm,
    index: usize,
    classes: &Classes,
) -> Result<(), UnscrambleError> {
    let method = &detfm.abc.methods[index];
    if method.code.is_empty() {
        return Ok(());
    }
    let Some(wrap_class) = &classes.wrap_class else {
        return Ok(());
    };

    let instructions: Vec<Instruction> = method.parse()?;
    let mut jump_info = JumpInfo::new(method, &instructions);
    let mut remove_calls = 0;
    let mut static_class: Option<&StaticClass> = None;
    for mut ins in instructions {
        if let Some(sclass) = static_class {
            if let Some(res) = handle_static_class(detfm, &ins.op, sclass) {
                (ins.op, ins.opcode) = res;
                jump_info.pop();
            }
            jump_info.add(ins);
            static_class = None;
            continue;
        }
        if wrap_class.is_wrap_ins(&ins) {
            // either CallProperty or GetProperty
            // in case of the later, we need to remove the next Call
            remove_calls += i32::from(ins.opcode == OpCode::GetProperty);
            jump_info.remove(&ins);
        } else if remove_calls > 0 && matches!(ins.opcode, OpCode::Call | OpCode::GetGlobalScope) {
            // remove the Call and the GetGlobalScope that is between GetProperty and Call
            remove_calls -= i32::from(ins.opcode == OpCode::Call);
            jump_info.remove(&ins);
        } else if let Op::GetLex(p) = &ins.op {
            match classes.static_classes.get(&p.property) {
                Some(sclass) => {
                    static_class = Some(sclass);
                    jump_info.add(ins); // remove the instruction later
                }
                None if p.property == wrap_class.name => {
                    jump_info.remove(&ins);
                }
                None => jump_info.add(ins),
            }
        } else {
            jump_info.add(ins);
        }
    }

    if jump_info.modified() {
        let method = &mut detfm.abc.methods[index];
        let new_instructions = jump_info.fix_addresses(method).unwrap();
        method.save_instructions(&new_instructions)?;
    }

    Ok(())
}

fn handle_static_class(detfm: &mut Detfm, op: &Op, sclass: &StaticClass) -> Option<(Op, OpCode)> {
    match op {
        Op::GetProperty(p) => match sclass.get_slot(detfm.abc, p.property) {
            Some(slot) => {
                let value = slot.index;
                Some(match slot.kind {
                    0x01 => (Op::PushString(PushStringArg { value }), OpCode::PushString),
                    0x06 => (Op::PushDouble(PushDoubleArg { value }), OpCode::PushDouble),
                    0x0A => (Op::PushFalse(), OpCode::PushFalse),
                    0x0B => (Op::PushTrue(), OpCode::PushTrue),
                    _ => unreachable!("Invalid slot kind"),
                })
            }
            None => None,
        },
        Op::CallProperty(p) => match sclass.methods.get(&p.property) {
            Some(nbr) => Some(nbr.get_op(detfm.cpool)),
            None => None,
        },
        _ => None,
    }
}
