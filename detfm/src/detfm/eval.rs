use rabc::abc::parser::OpCode;
use std::{collections::HashMap, sync::LazyLock};

#[derive(Debug, Clone)]
pub enum StackValue {
    Invalid(),
    Boolean(bool),
    Number(f64),
    String(String),
}

impl StackValue {
    #[must_use]
    pub fn op(self, other: StackValue, opcode: OpCode) -> Self {
        match (self, other, opcode) {
            (Self::Number(a), Self::Number(b), OpCode::Add) => (a + b).into(),
            (Self::Number(a), Self::Number(b), OpCode::Subtract) => (a - b).into(),
            (Self::Number(a), Self::Number(b), OpCode::Multiply) => (a * b).into(),
            (Self::Number(a), Self::Number(b), OpCode::Divide) => (a / b).into(),
            (Self::String(a), Self::String(b), OpCode::Add) => (a + &b).into(),
            _ => Self::Invalid(), // Don't bother trying
        }
    }

    #[must_use]
    pub fn to_bool(&self) -> StackValue {
        Self::Boolean(match self {
            StackValue::Invalid() => false,
            StackValue::Boolean(value) => *value,
            StackValue::Number(value) => *value != 0.0,
            StackValue::String(value) => !value.is_empty(),
        })
    }
}

impl From<bool> for StackValue {
    fn from(value: bool) -> Self {
        Self::Boolean(value)
    }
}
impl From<i8> for StackValue {
    fn from(value: i8) -> Self {
        Self::Number(value.into())
    }
}
impl From<i16> for StackValue {
    fn from(value: i16) -> Self {
        Self::Number(value.into())
    }
}
impl From<i32> for StackValue {
    fn from(value: i32) -> Self {
        Self::Number(value.into())
    }
}
impl From<u32> for StackValue {
    fn from(value: u32) -> Self {
        Self::Number(value.into())
    }
}
impl From<f64> for StackValue {
    fn from(value: f64) -> Self {
        Self::Number(value)
    }
}
impl From<String> for StackValue {
    fn from(value: String) -> Self {
        Self::String(value)
    }
}

pub(crate) static STACK_OPERATIONS: LazyLock<HashMap<OpCode, [u32; 2]>> = LazyLock::new(|| {
    HashMap::from_iter([
        // { opcode, {take from stack, put on stack}}
        // stack indenpendent
        (OpCode::Jump, [0, 0]),
        (OpCode::Kill, [0, 0]),
        (OpCode::PopScope, [0, 0]),
        (OpCode::ReturnVoid, [0, 0]),
        (OpCode::ReturnVoid, [0, 0]),
        // add to the stack
        (OpCode::GetLocal0, [0, 1]),
        (OpCode::GetLex, [0, 1]),
        (OpCode::PushNull, [0, 1]),
        (OpCode::FindProperty, [0, 1]),
        (OpCode::FindPropstrict, [0, 1]),
        (OpCode::NewArray, [0, 1]),
        (OpCode::NewFunction, [0, 1]),
        (OpCode::NewCatch, [0, 2]), // should be 1, but also add the error to the stack
        // take from the stack
        (OpCode::SetLocal1, [1, 0]),
        (OpCode::PushScope, [1, 0]),
        (OpCode::Pop, [1, 0]),
        (OpCode::CallPropVoid, [1, 0]),
        (OpCode::IfFalse, [1, 0]),
        (OpCode::IfTrue, [1, 0]),
        (OpCode::IfEq, [2, 0]),
        (OpCode::SetProperty, [2, 0]),
        (OpCode::InitProperty, [2, 0]),
        (OpCode::SetSlot, [2, 0]),
        // hybrid
        (OpCode::GetProperty, [1, 1]),
        (OpCode::ConstructProp, [1, 1]),
        (OpCode::Construct, [1, 1]),
        (OpCode::ApplyType, [1, 1]),
        (OpCode::CallProperty, [1, 1]),
        (OpCode::Equals, [2, 1]),
    ])
});
