macro_rules! formatter {
    ($name:ident, $fmt: literal, $($arg:ident: $type:ty),*) => {
        pub fn $name($($arg: $type),*) -> String {
            format!($fmt, $($arg),*)
        }
    };
}

formatter!(_test, "test_{}", i: u8);

formatter!(classes,  "class_{:03}", i: u32);
formatter!(consts,  "const_{:03}", i: u32);
formatter!(functions, "function_{:03}", i: u32);
formatter!(_names,  "name_{:03}", i: u32);
formatter!(vars,  "var_{:03}", i: u32);
formatter!(methods,  "method_{:03}", i: u32);
formatter!(errors,  "error{:}", i: u32);
formatter!(symbols,  "ClassSymbol_{:}", i: u32);

formatter!(clientbound_packet  , "CPacket{:02x}{:02x}{}", categ_id: u8, pkt_id: u8, name: String);
formatter!(serverbound_packet  , "SPacket{:02x}{:02x}{}", categ_id: u8, pkt_id: u8, name: String);
formatter!(tribulle_clientbound_packet, "TCPacket_{:04x}{}", i: u16, name: String);
formatter!(tribulle_serverbound_packet, "TSPacket_{:04x}{}", i: u16, name: String);

formatter!(packet_subhandler, "PacketSubHandler_{:02x}", i: u16);
formatter!(unknown_clientbound_packet, "CPacket_u{:02}", i: u16);
