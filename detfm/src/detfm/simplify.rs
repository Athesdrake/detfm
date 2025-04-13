use super::{
    eval::{StackValue, STACK_OPERATIONS},
    jump_info::{JumpInfo, JumpInfoError},
};
use anyhow::Result;
use rabc::{
    abc::{
        constant_pool::PushGetIndex,
        parser::{
            opargs::{PushByteArg, PushDoubleArg, PushShortArg, PushStringArg},
            Instruction, Op, OpCode,
        },
        ConstantPool, Method,
    },
    error::RabcError,
};
use thiserror::Error;

#[derive(Error, Debug)]
pub enum SimplifyError {
    #[error("Cannot pop value from an empty stack.")]
    EmptyStack,
    #[error("Unsupported opcode: {0}")]
    UnsupportedOpCode(OpCode),
    #[error(transparent)]
    JumpInfoError(#[from] JumpInfoError),
    #[error(transparent)]
    RabcError(#[from] RabcError),
}

pub fn simplify_expressions(
    cpool: &mut ConstantPool,
    method: &mut Method,
) -> Result<(), SimplifyError> {
    let instructions = method.parse()?;
    let mut stack: Vec<StackValue> = Vec::with_capacity(method.max_stack as usize);
    let mut modified = false;
    let mut jump_info = JumpInfo::new(method, &instructions);
    for ins in &instructions {
        let mut new_ins = ins.clone();
        match &ins.op {
            #[allow(clippy::cast_possible_wrap)]
            Op::PushByte(arg) => stack.push((arg.value as i8).into()),
            Op::PushShort(arg) => stack.push(arg.value.into()),
            Op::PushInt(arg) => stack.push((*cpool.get_int(arg.value)?).into()),
            Op::PushUint(arg) => stack.push((*cpool.get_uint(arg.value)?).into()),
            Op::PushDouble(arg) => stack.push((*cpool.get_double(arg.value)?).into()),
            Op::PushTrue() => stack.push(true.into()),
            Op::PushFalse() => stack.push(false.into()),
            Op::PushString(arg) => stack.push(cpool.get_str(arg.value)?.clone().into()),
            Op::Dup() => stack.push(stack.last().ok_or(SimplifyError::EmptyStack)?.clone()),
            Op::Swap() => {
                let length = stack.len();
                if length < 2 {
                    return Err(SimplifyError::EmptyStack);
                }
                stack.swap(length - 1, length - 2);
            }
            Op::Not() => match stack.last_mut().ok_or(SimplifyError::EmptyStack)? {
                StackValue::Boolean(value) => *value = !*value,
                value => *value = StackValue::Invalid(),
            },
            Op::Subtract() | Op::Multiply() | Op::Divide() | Op::Add() => {
                let a = stack.pop().ok_or(SimplifyError::EmptyStack)?;
                let b = stack.pop().ok_or(SimplifyError::EmptyStack)?;
                let value = a.op(b, ins.opcode);
                if !matches!(value, StackValue::Invalid()) {
                    modified = true;
                    // remove_ins(&mut new_instructions, &mut new_ins, &mut jump_info, 2);
                    jump_info.pop();
                    jump_info.pop();
                    edit_ins(cpool, &mut new_ins, &value);
                }
                stack.push(value);
            }
            Op::Negate() => {
                if let Some(stack_value) = stack.last_mut() {
                    if let StackValue::Number(value) = stack_value {
                        *value = -*value;
                        modified = true;
                        // remove_ins(&mut new_instructions, &mut new_ins, &mut jump_info, 1);
                        jump_info.pop();
                        edit_ins(cpool, &mut new_ins, stack_value);
                    }
                }
            }
            Op::CallProperty(arg)
                if arg.arg_count == 1
                    && cpool.str(arg.property).is_some_and(|n| n == "Boolean") =>
            {
                let value = stack.pop().ok_or(SimplifyError::EmptyStack)?;
                modified = true;
                // remove_ins(&mut new_instructions, &mut new_ins, &mut jump_info, 2);
                jump_info.pop();
                jump_info.pop();
                edit_ins(cpool, &mut new_ins, &value);
                stack.push(value.to_bool());
            }
            op => {
                let Some([mut take, put]) = STACK_OPERATIONS.get(&ins.opcode) else {
                    return Err(SimplifyError::UnsupportedOpCode(ins.opcode));
                };
                // dynamic stack opcodes
                take += match op {
                    Op::Construct(n) | Op::ApplyType(n) | Op::NewArray(n) => n.arg_count,
                    Op::ConstructProp(n) | Op::CallProperty(n) | Op::CallPropVoid(n) => n.arg_count,
                    _ => 0,
                };

                // consume stack values
                stack.truncate(match stack.len().checked_sub(take as usize) {
                    Some(length) => length,
                    _ => return Err(SimplifyError::EmptyStack),
                });
                // push new stack values
                for _ in 0..*put {
                    stack.push(StackValue::Invalid());
                }
            }
        }
        // new_instructions.push(new_ins);
        jump_info.add(new_ins);
    }

    if modified {
        let new_instructions = jump_info.fix_addresses(method)?;
        method.save_instructions(&new_instructions)?;
    }
    Ok(())
}

/* fn remove_ins(
    instructions: &mut Vec<Instruction>,
    ins: &mut Instruction,
    jump_info: &mut JumpInfo,
    to_remove: usize,
) {
    for _ in 0..to_remove {
        if let Some(before) = instructions.pop() {
            jump_info.remove(before.addr, ins.addr);
            // ins.jumps_here.extend(before.jumps_here);
        }
    }
} */
fn edit_ins(cpool: &mut ConstantPool, ins: &mut Instruction, value: &StackValue) {
    match value {
        StackValue::Number(value) => {
            if value.fract() != 0.0 || value.abs() > 0x8000.into() {
                ins.opcode = OpCode::PushDouble;
                ins.op = Op::PushDouble(PushDoubleArg {
                    value: cpool.doubles.pushi(*value),
                });
            } else if value.abs() > 0x80.into() {
                ins.opcode = OpCode::PushShort;
                ins.op = Op::PushShort(PushShortArg {
                    value: *value as i16,
                });
            } else {
                ins.opcode = OpCode::PushByte;
                ins.op = Op::PushByte(PushByteArg {
                    value: *value as i8 as u8,
                });
            }
        }
        StackValue::String(value) => {
            ins.opcode = OpCode::PushString;
            ins.op = Op::PushString(PushStringArg {
                value: cpool.strings.pushi(value.to_owned()),
            });
        }
        StackValue::Boolean(value) => {
            ins.opcode = if *value {
                OpCode::PushTrue
            } else {
                OpCode::PushFalse
            };
            ins.op = if *value {
                Op::PushTrue()
            } else {
                Op::PushFalse()
            };
        }
        StackValue::Invalid() => {
            panic!("Invalid stack value.")
        }
    }
}
