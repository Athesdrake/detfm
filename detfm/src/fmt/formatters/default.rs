use crate::{detfm::pktnames::PktNames, fmt::Formatter, renamer::Counters};
use rabc::abc::Trait;

#[derive(Debug)]
pub struct DefaultFormatter;

impl Formatter for DefaultFormatter {
    fn traits(&self, ctrait: &Trait, counters: &mut Counters) -> String {
        match ctrait {
            Trait::Const(_) => format!("const_{:03}", counters.consts()),
            Trait::Method(_) => format!("method_{:03}", counters.methods()),
            Trait::Function(_) => format!("function_{:03}", counters.functions()),
            _ => format!("var_{:03}", counters.vars()),
        }
    }
    fn classes(&self, counter: u32) -> String {
        format!("class_{counter:03}")
    }
    fn errors(&self, counter: u32) -> String {
        format!("error{counter:}")
    }
    fn symbols(&self, id: u16) -> String {
        format!("ClassSymbol_{id:}")
    }
    fn packets(&self, side: PktNames, pkt_id: u16, name: String) -> String {
        let (categ_id, id) = (pkt_id >> 8, pkt_id & 0xff);
        match side {
            PktNames::Serverbound => format!("SPacket{categ_id:02x}{id:02x}{name}"),
            PktNames::Clientbound => format!("CPacket{categ_id:02x}{id:02x}{name}"),
            PktNames::TribulleClientbound => format!("TCPacket_{pkt_id:04x}{name}"),
            PktNames::TribulleServerbound => format!("TSPacket_{pkt_id:04x}{name}"),
        }
    }
    fn subhandler(&self, category: u8) -> String {
        format!("PacketSubHandler_{category:02x}")
    }
    fn unknown_packet(&self, counter: u32) -> String {
        format!("CPacket_u{counter:02x}")
    }
}
