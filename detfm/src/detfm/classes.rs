use anyhow::{bail, Result};
use rabc::{
    abc::{
        constant_pool::PushGetIndex,
        parser::{
            opargs::{PushDoubleArg, PushIntArg},
            InsIter, Instruction, Op, OpCode,
        },
        r#trait::SlotTrait,
        Class, ConstantPool, Method, Trait,
    },
    Abc,
};
use std::{
    collections::{HashMap, HashSet, VecDeque},
    ops::Add,
};

#[derive(Debug)]
pub struct WrapClass {
    pub index: usize,
    pub name: u32,
    methods: HashSet<u32>,
}

impl WrapClass {
    pub fn new(index: usize, class: &Class) -> Self {
        Self {
            index,
            name: class.name,
            methods: class.ctraits.iter().map(Trait::name).collect(),
        }
    }

    pub fn is_wrap_ins(&self, ins: &Instruction) -> bool {
        self.methods.contains(&match &ins.op {
            Op::CallProperty(p) => p.property,
            Op::GetProperty(p) => p.property,
            _ => return false,
        })
    }
}

#[derive(Debug, Default)]
pub struct StaticClass {
    pub class_index: usize,
    pub slots: HashMap<u32, usize>,
    pub methods: HashMap<u32, StaticClassNumber>,
}
impl StaticClass {
    pub fn new(class_index: usize) -> Self {
        Self {
            class_index,
            ..Self::default()
        }
    }
    pub fn from_class(abc: &mut Abc, cpool: &mut ConstantPool, class_index: usize) -> Self {
        let mut res = Self::new(class_index);
        let mut notdefined = HashMap::new();
        for (i, ctrait) in abc.classes[class_index].ctraits.iter().enumerate() {
            match ctrait {
                Trait::Slot(slot) => {
                    res.slots.insert(slot.name, i);
                    if slot.kind == 0x00 {
                        notdefined.insert(slot.name, i);
                    }
                }
                Trait::Method(m) => {
                    let method = abc.get_method(m.index).unwrap();
                    let return_type = cpool.qname(method.return_type);
                    let value = match return_type {
                        Some(t) if t == "int" => {
                            StaticClassNumber::Int(eval_method(cpool, method).unwrap())
                        }
                        Some(t) if t == "Number" => {
                            StaticClassNumber::Float(eval_method(cpool, method).unwrap())
                        }
                        _ => panic!("Unknown return type: {return_type:?}"),
                    };
                    res.methods.insert(m.name, value);
                }
                _ => unreachable!("Not a static class"),
            }
        }

        if notdefined.is_empty() {
            return res;
        }
        let instructions = abc
            .get_method(abc.classes[class_index].cinit)
            .unwrap()
            .parse()
            .unwrap();
        let mut prog = instructions.iter_prog();
        while prog.has_next() {
            let Some(Op::FindProperty(prop)) = prog.skip_until_op(&OpCode::FindProperty) else {
                break;
            };
            let Some(i) = notdefined.remove(&prop.property) else {
                prog.next();
                continue;
            };
            let Trait::Slot(slot) = &mut abc.classes[class_index].ctraits[i] else {
                unreachable!()
            };
            match prog.next_op() {
                Some(Op::PushTrue()) => slot.kind = 0x0A,
                Some(Op::PushFalse()) => slot.kind = 0x0B,
                _ => drop(res.slots.remove(&slot.name)),
            }
        }
        res
    }

    pub fn get_slot<'a>(&self, abc: &'a Abc, property: u32) -> Option<&'a SlotTrait> {
        self.slots.get(&property).map(|&index| {
            let Trait::Slot(slot) = &abc.classes[self.class_index].ctraits[index] else {
                unreachable!("Invalid property");
            };
            slot
        })
    }
}

fn eval_method<T>(cpool: &mut ConstantPool, method: &Method) -> Result<T>
where
    T: From<u8> + From<i32> + Add<Output = T> + std::ops::Div<Output = T>,
{
    let mut stack: VecDeque<T> = VecDeque::with_capacity(method.max_stack as usize);
    for ins in method.parse()? {
        match ins.op {
            Op::GetLocal0() | Op::PushScope() => {}
            Op::PushByte(p) => stack.push_back(p.value.into()),
            Op::PushShort(p) => stack.push_back(i32::from(p.value).into()),
            Op::PushInt(p) => stack.push_back((*cpool.get_int(p.value)?).into()),
            Op::Add() => {
                let value = match (stack.pop_back(), stack.pop_back()) {
                    (Some(a), Some(b)) => a + b,
                    _ => bail!("Cannot pop value from an empty stack."),
                };
                stack.push_back(value);
            }
            Op::Divide() => {
                let value = match (stack.pop_back(), stack.pop_back()) {
                    (Some(a), Some(b)) => a / b,
                    _ => bail!("Cannot pop value from an empty stack."),
                };
                stack.push_back(value);
            }
            Op::ReturnValue() => match stack.pop_back() {
                Some(value) => return Ok(value),
                None => bail!("Cannot pop value from an empty stack."),
            },
            op => bail!("Unsupported operation {op:?}"),
        }
    }
    bail!("Nothing to return")
}
#[derive(Debug)]
pub enum StaticClassNumber {
    Int(i32),
    Float(f64),
}

impl StaticClassNumber {
    pub fn get_op(&self, cpool: &mut ConstantPool) -> (Op, OpCode) {
        match self {
            StaticClassNumber::Int(val) => {
                let value = cpool.integers.pushi(*val);
                (Op::PushInt(PushIntArg { value }), OpCode::PushInt)
            }
            StaticClassNumber::Float(val) => {
                let value = cpool.doubles.pushi(*val);
                (Op::PushDouble(PushDoubleArg { value }), OpCode::PushDouble)
            }
        }
    }
}

#[allow(clippy::struct_field_names)]
#[derive(Debug, Default)]
pub struct Classes {
    pub wrap_class: Option<WrapClass>,
    pub base_spkt: Option<usize>,
    pub base_cpkt: Option<usize>,
    pub pkt_hdlr: Option<usize>,
    pub varint_reader: Option<usize>,
    pub interface_proxy: Option<usize>,
    pub static_classes: HashMap<u32, StaticClass>,
}
#[derive(Debug)]
pub enum DetfmClass {
    WrapClass,
    BaseSpkt,
    BaseCpkt,
    PktHdlr,
    VarintReader,
    InterfaceProxy,
}

impl std::fmt::Display for DetfmClass {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str(match self {
            Self::WrapClass => "Wrapper Class",
            Self::BaseSpkt => "Serverbound Base Packet",
            Self::BaseCpkt => "Clientbound Base Packet",
            Self::PktHdlr => "Packet Handler Class",
            Self::VarintReader => "VarInt Reader Class",
            Self::InterfaceProxy => "Interface Proxy Class",
        })
    }
}
