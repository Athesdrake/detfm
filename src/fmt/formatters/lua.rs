use crate::{detfm::pktnames::PktNames, fmt::Formatter, renamer::Counters};
use mlua::{prelude::*, Function};
use rabc::abc::Trait;

use super::DefaultFormatter;

#[derive(Debug)]
pub struct LuaFormatter {
    #[allow(dead_code)]
    lua: Lua,
    fmt: DefaultFormatter,
    format_const: Option<Function>,
    format_method: Option<Function>,
    format_function: Option<Function>,
    format_vars: Option<Function>,
    format_class: Option<Function>,
    format_error: Option<Function>,
    format_symbol: Option<Function>,
    format_packet: Option<Function>,
    format_subhandler: Option<Function>,
    format_unknown_packet: Option<Function>,
}

impl LuaFormatter {
    pub fn from_script(script: String) -> mlua::Result<Self> {
        let lua = Lua::new();
        lua.load(script).exec()?;
        let globals = lua.globals();
        Ok(Self {
            lua,
            fmt: DefaultFormatter,
            format_const: globals.get("format_const")?,
            format_method: globals.get("format_method")?,
            format_function: globals.get("format_function")?,
            format_vars: globals.get("format_vars")?,
            format_class: globals.get("format_class")?,
            format_error: globals.get("format_error")?,
            format_symbol: globals.get("format_symbol")?,
            format_packet: globals.get("format_packet")?,
            format_subhandler: globals.get("format_subhandler")?,
            format_unknown_packet: globals.get("format_unknown_packet")?,
        })
    }
}

impl IntoLua for PktNames {
    fn into_lua(self, lua: &Lua) -> LuaResult<LuaValue> {
        match self {
            Self::Serverbound => "serverbound",
            Self::Clientbound => "clientbound",
            Self::TribulleClientbound => "tribulle_clientbound",
            Self::TribulleServerbound => "tribulle_serverbound",
        }
        .into_lua(lua)
    }
}
impl IntoLua for &PktNames {
    fn into_lua(self, lua: &Lua) -> LuaResult<LuaValue> {
        (*self).into_lua(lua)
    }
}

macro_rules! callf {
    ($func:expr, $($args:expr),* => $fallback:expr) => {
        match &$func {
            Some(func) => func.call(($($args,)*)).unwrap(),
            None => $fallback,
        }
    };
}

impl Formatter for LuaFormatter {
    fn traits(&self, ctrait: &Trait, counters: &mut Counters) -> String {
        let (func, counter): (_, fn(&mut Counters) -> u32) = match ctrait {
            Trait::Const(_) => (&self.format_const, Counters::consts),
            Trait::Method(_) => (&self.format_method, Counters::methods),
            Trait::Function(_) => (&self.format_function, Counters::functions),
            _ => (&self.format_vars, Counters::vars),
        };
        callf!(func, counter(counters) => self.fmt.traits(ctrait, counters))
    }
    fn classes(&self, counter: u32) -> String {
        callf!(self.format_class, counter => self.fmt.classes(counter))
    }
    fn errors(&self, counter: u32) -> String {
        callf!(self.format_error, counter => self.fmt.errors(counter))
    }
    fn symbols(&self, id: u16) -> String {
        callf!(self.format_symbol, id => self.fmt.symbols(id))
    }
    fn packets(&self, side: &PktNames, pkt_id: u16, name: String) -> String {
        let (categ_id, id) = (pkt_id >> 8, pkt_id & 0xff);
        callf!(self.format_packet, side, categ_id, id, name => self.fmt.packets(side, pkt_id, name))
    }
    fn subhandler(&self, category: u8) -> String {
        callf!(self.format_subhandler, category => self.fmt.subhandler(category))
    }
    fn unknown_packet(&self, counter: u32) -> String {
        callf!(self.format_unknown_packet, counter => self.fmt.unknown_packet(counter))
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use rabc::abc::r#trait::SlotTrait;

    const SCRIPT: &str = r#"
function format_const(counter)
    return string.format("lua_const_%s", counter)
end
"#;
    #[test]
    fn test_new_lua() {
        let fmt = LuaFormatter::from_script(SCRIPT.to_owned()).unwrap();
        let mut counters = Counters::default();
        for _ in 0..42 {
            counters.consts();
        }

        let ctrait = Trait::Const(SlotTrait::default());
        assert_eq!(fmt.traits(&ctrait, &mut counters), "lua_const_42");
        assert_eq!(fmt.errors(69), "error69");
    }
}
