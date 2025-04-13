use crate::{
    fmt::Formatter,
    rename::{PoolRenamer, Rename},
};
use anyhow::{anyhow, bail, Result};
use classes::{Classes, DetfmClass, StaticClass, WrapClass};
use itertools::Itertools;
use ns::NsNames;
use pktnames::{PacketNames, PktNames, PKT_NAMES};
use rabc::{
    abc::{
        constant_pool::PushGetIndex,
        namespace::NamespaceKind,
        parser::{
            opargs::{PropertyArg, PushDoubleArg},
            opmatch::OpSeq,
            InsIter, InsIterator, Op, OpCode,
        },
        Class, ConstantPool, Metadata, Method, Multiname, Namespace, Trait, TraitAttr,
    },
    Abc,
};
use simplify::simplify_expressions;
use std::{
    collections::{hash_map::Entry, HashMap, HashSet},
    mem::swap,
    option::Option,
};
use unscramble::unscramble_method;

mod classes;
mod jump_info;

pub mod eval;
pub mod ns;
pub mod pktnames;
pub mod simplify;
pub mod unscramble;

static NEW_CLASS_SEQ: [OpCode; 3] = [
    OpCode::FindPropstrict,
    OpCode::GetLocal1,
    OpCode::ConstructProp,
];
static SUB_HANDLER_SEQ: [OpCode; 6] = [
    OpCode::GetLex,
    OpCode::GetLocal1,
    OpCode::GetLex,
    OpCode::GetProperty,
    OpCode::CallPropVoid,
    OpCode::ReturnVoid,
];
static TRIBULLE_PKT_GETTER_SEQ: OpSeq<5> = OpSeq([
    OpCode::GetLex,
    OpCode::GetLex,
    OpCode::GetProperty,
    OpCode::CallProperty,
    OpCode::Coerce,
]);
static TRIBULLE_PKT_RETURN_ID_SEQ: [OpCode; 3] =
    [OpCode::Label, OpCode::PushDouble, OpCode::ReturnValue];

pub struct Detfm<'a> {
    abc: &'a mut Abc,
    cpool: &'a mut ConstantPool,

    byte_array: Option<u32>,
    ns_class_map: HashMap<u32, u32>,

    pub fmt: Box<dyn Formatter>,
    packet_names: PacketNames,
}

impl<'a> Detfm<'a> {
    pub fn new(
        abc: &'a mut Abc,
        cpool: &'a mut ConstantPool,
        fmt: Box<dyn Formatter>,
        packet_names: PacketNames,
    ) -> Self {
        Self {
            abc,
            cpool,
            byte_array: None,
            ns_class_map: HashMap::new(),
            packet_names,
            fmt,
        }
    }

    pub fn simplify_init(&mut self) -> Result<()> {
        for cls in &self.abc.classes {
            let Some(method) = self.abc.methods.get_mut(cls.cinit as usize) else {
                bail!("Cannot find class's init method.");
            };
            simplify_expressions(self.cpool, method)?;
        }
        Ok(())
    }

    pub fn unscramble(&mut self, classes: &Classes) -> Result<()> {
        for method in 0..self.abc.methods.len() {
            unscramble_method(self, method, classes)?;
        }
        Ok(())
    }

    pub fn analyze(&mut self) -> (Classes, Vec<String>) {
        type FnCheck = fn(&Detfm<'_>, &Class) -> bool;
        let is_bytearray = |mn: &&Multiname| {
            let Multiname::QName(qname) = mn else {
                return false;
            };
            matches!(self.cpool.get_str(qname.name), Ok(name) if name == "ByteArray")
        };

        if let Some((i, _)) = self.cpool.multinames.iter().find_position(is_bytearray) {
            self.byte_array = Some(i as u32)
        }

        let mut classes = Classes::default();
        let mut static_classes = HashMap::new();
        let mut to_find: Vec<(DetfmClass, FnCheck)> = vec![
            (DetfmClass::WrapClass, |d, c| d.is_wrap_class(c)),
            (DetfmClass::BaseCpkt, |d, c| d.is_clientbound_pkt(c)),
            (DetfmClass::BaseSpkt, |d, c| d.is_serverbound_pkt(c)),
            (DetfmClass::InterfaceProxy, |d, c| d.is_interface_proxy(c)),
            (DetfmClass::PktHdlr, |d, c| d.is_packet_handler(c)),
            (DetfmClass::VarintReader, |d, c| d.is_varint_reader(c)),
        ];
        for (i, class) in self.abc.classes.iter().enumerate() {
            if self.is_slot_class(class) {
                static_classes.insert(i, class.name);
            } else if let Some(index) = to_find.iter().position(|(_, f)| f(self, class)) {
                log::debug!("Found {} {}", to_find[index].0, i);
                match to_find.remove(index).0 {
                    DetfmClass::WrapClass => classes.wrap_class = Some(WrapClass::new(i, class)),
                    DetfmClass::BaseCpkt => classes.base_cpkt = Some(i),
                    DetfmClass::BaseSpkt => classes.base_spkt = Some(i),
                    DetfmClass::PktHdlr => classes.pkt_hdlr = Some(i),
                    DetfmClass::VarintReader => classes.varint_reader = Some(i),
                    DetfmClass::InterfaceProxy => classes.interface_proxy = Some(i),
                }
            }
        }
        for (i, name) in static_classes {
            let sclass = StaticClass::from_class(self.abc, self.cpool, i);
            classes.static_classes.insert(name, sclass);
        }

        let mut missings = Vec::new();
        if self.byte_array.is_none() {
            missings.push("ByteArray Multiname".to_owned());
        }
        if classes.static_classes.is_empty() {
            missings.push("Static Classes".to_owned());
        }

        for (cls, _) in to_find {
            missings.push(cls.to_string());
        }
        (classes, missings)
    }

    pub fn rename(&mut self, classes: Classes) -> Result<()> {
        let ns = NsNames {
            slot: self.create_package("com.obfuscate"),
            pkt: self.create_package("packets"),
            spkt: self.create_package("packets.serverbound"),
            cpkt: self.create_package("packets.clientbound"),
            tpkt: self.create_package("packets.tribulle"),
            tspkt: self.create_package("packets.tribulle.serverbound"),
            tcpkt: self.create_package("packets.tribulle.clientbound"),
        };

        if let Some(index) = classes.base_spkt {
            self.set_class_ns(index, ns.pkt)?;
            let class = &self.abc.classes[index];
            class.rename_str(self.cpool, "SPacketBase")?;
            class.itraits[2].rename_str(self.cpool, "pcode")?;
        }

        let meta_index = self.abc.metadatas.len() as u32;
        self.abc.metadatas.push(Metadata {
            name: self.cpool.strings.pushi("buffer".to_owned()),
            items: vec![],
        });
        if let Some(index) = classes.base_cpkt {
            self.set_class_ns(index, ns.pkt)?;
            let class = &mut self.abc.classes[index];
            class.rename_str(self.cpool, "CPacketBase")?;
            class.itraits[0].rename_str(self.cpool, "pcode0")?;
            class.itraits[1].rename_str(self.cpool, "pcode1")?;
            class.itraits[2].rename_str(self.cpool, "buffer")?;
            class.itraits[2].metadatas_mut().push(meta_index);
        }
        if let Some(index) = classes.varint_reader {
            self.set_class_ns(index, ns.pkt)?;
            let class = &mut self.abc.classes[index];
            class.rename_str(self.cpool, "VarIntReader")?;
            class.itraits[0].rename_str(self.cpool, "buffer")?;
            class.itraits[0].metadatas_mut().push(meta_index);
        }

        self.rename_writeany(&classes)?;
        self.rename_readany(&classes)?;
        self.rename_interface_proxy(&classes)?;

        let simple_push = OpSeq([OpCode::PushDouble, OpCode::ConstructSuper]);
        let double_push = OpSeq([
            OpCode::PushDouble,
            OpCode::PushDouble,
            OpCode::ConstructSuper,
        ]);
        let super_construct_seq = double_push | simple_push;

        if let (Some(base_spkt), Some(base_cpkt)) = (classes.base_spkt, classes.base_cpkt) {
            let spkt_name = self.cpool.str(self.abc.classes[base_spkt].name).unwrap();
            let cpkt_name = self.cpool.str(self.abc.classes[base_cpkt].name).unwrap();

            let mut clientbound_counter = 0;
            let mut to_rename = Vec::new();
            for (index, klass) in self.abc.classes.iter().enumerate() {
                if klass.super_name == 0 {
                    continue;
                }

                let super_name = self.cpool.str(klass.super_name).unwrap();
                if super_name == spkt_name {
                    let instructions = self.abc.get_method(klass.iinit)?.parse()?;
                    let mut prog = instructions.iter_prog();
                    let get_code = |p: &PushDoubleArg| {
                        self.cpool.get_double(p.value).cloned().unwrap_or(0.0) as u8
                    };

                    // Find the Packet's code: super(categ_id, pkt_id)
                    let (categ_id, pkt_id) = match prog.skip_until_match(&super_construct_seq)[..] {
                        [Op::PushDouble(p1), Op::PushDouble(p2), _] => (get_code(p1), get_code(p2)),
                        [Op::PushDouble(p1), _] => (0, get_code(p1)),
                        _ => bail!(
                            "Unable to find serverbound packet's code: {}",
                            self.cpool.fqn(klass).unwrap_or(index.to_string())
                        ),
                    };
                    let name = self.format_packet2(&PktNames::Serverbound, categ_id, pkt_id);
                    klass.rename(self.cpool, name)?;
                    to_rename.push((index, ns.spkt));
                } else if super_name == cpkt_name {
                    clientbound_counter += 1;
                    let name = self.fmt.unknown_packet(clientbound_counter);
                    klass.rename(self.cpool, name)?;
                    to_rename.push((index, ns.cpkt));
                }
            }
            // set the namespace on the classes
            for (index, ns) in to_rename {
                self.set_class_ns(index, ns)?;
            }
        }

        // Clear all static classes
        for cls in classes.static_classes.values() {
            let index = cls.class_index;
            let class = self.abc.classes.get_mut(index).unwrap();
            let cinit = class.cinit as usize;
            class.ctraits.clear();
            class.itraits.clear();
            class.rename(self.cpool, format!("$StaticClass_{index:04}"))?;
            self.set_class_ns(index, ns.slot)?;
            self.abc.methods.get_mut(cinit).unwrap().code.clear();
        }
        if let Some(wrap_class) = &classes.wrap_class {
            let class = self.abc.classes.get_mut(wrap_class.index).unwrap();
            class.ctraits.clear();
            class.itraits.clear();
            class.rename_str(self.cpool, "$WrapperClass")?;
            self.set_class_ns(wrap_class.index, ns.slot)?;
        }

        // Rename the instance trait
        let game_class = &self.abc.classes[0];
        for t in &game_class.ctraits {
            if let Trait::Slot(slot) = t {
                if slot.slot_type == game_class.name {
                    t.rename_str(self.cpool, "instance")?;
                    break;
                }
            }
        }

        self.find_clientbound_packets(&classes, &ns)?;
        self.create_missing_sets();
        Ok(())
    }

    pub fn proxy2localhost(&mut self, port: u16) -> Option<(String, String)> {
        for string in &mut self.cpool.strings {
            // min size should be: 0.0.0.0:0-0
            if string.len() < 11 {
                continue;
            }
            let Some((left, right)) = string.split_once(':') else {
                continue;
            };
            if left.chars().all(|c| c.is_ascii_digit() || c == '.')
                && right.chars().all(|c| c.is_ascii_digit() || c == '-')
                && right.chars().any(|c| c == '-')
            {
                let mut previous = format!("127.0.0.1:{port}");
                swap(&mut previous, string);
                return Some((previous, string.clone()));
            }
        }
        None
    }

    fn create_package(&mut self, name: &str) -> u32 {
        self.cpool.namespaces.pushi(Namespace {
            kind: NamespaceKind::Package,
            name: self.cpool.strings.pushi(name.to_owned()),
        })
    }

    fn create_missing_sets(&mut self) {
        let mut ns_set_cache: HashMap<u32, u32> = HashMap::new();
        for mn in &mut self.cpool.multinames {
            match mn {
                Multiname::QName(qname) | Multiname::QNameA(qname) => {
                    if let Some(ns) = self.ns_class_map.get(&qname.name) {
                        qname.ns = *ns;
                    }
                }
                Multiname::Multiname(mn) => {
                    if let Some(&ns) = self.ns_class_map.get(&mn.name) {
                        // Also change the namespace on the other multinames using the same name
                        if let Entry::Vacant(entry) = ns_set_cache.entry(ns) {
                            entry.insert(self.cpool.ns_sets.pushi(vec![ns]));
                        }
                        mn.ns_set = ns_set_cache[&ns];
                    }
                }
                _ => {}
            }
        }
    }

    fn set_class_ns(&mut self, index: usize, ns: u32) -> Result<()> {
        assert!(ns != 0, "Cannot rename class namespace to null.");
        let class = &self.abc.classes[index];
        let mn = self.cpool.get_mn_mut(class.name)?;
        match mn {
            Multiname::QName(qname) | Multiname::QNameA(qname) => {
                qname.ns = ns;
                if qname.name != 0 {
                    self.ns_class_map.insert(qname.name, ns);
                }
            }
            _ => bail!("Invalid multiname for a class: {mn:?}"),
        }
        Ok(())
    }

    fn rename_writeany(&mut self, classes: &Classes) -> Result<()> {
        let Some(index) = classes.base_spkt else {
            return Ok(()); // ignore
        };

        let base_spkt = &self.abc.classes[index];
        for t in &base_spkt.itraits {
            let Ok(method) = self.get_method_from_trait(t) else {
                continue;
            };
            if method.max_stack != method.local_count
                || method.max_stack > 2
                || method.init_scope_depth != method.max_scope_depth - 1
                || method.return_type != base_spkt.name
            {
                continue;
            }

            let instructions = method.parse()?;
            let allowed: HashSet<_> = HashSet::from_iter([
                OpCode::GetLocal0,
                OpCode::GetLocal1,
                OpCode::PushScope,
                OpCode::ReturnValue,
            ]);
            let mut name = 0;
            for ins in instructions {
                match &ins.op {
                    Op::GetProperty(p) => {
                        // Make sure we get the buffer
                        if p.property != base_spkt.itraits[0].name() {
                            break;
                        }
                    }
                    Op::CallPropVoid(p) => {
                        if name == 0 {
                            name = self.cpool.get_mn(p.property)?.get_name_index().unwrap();
                        } else {
                            // two names !!
                            name = 0;
                            break;
                        }
                    }
                    _ if !allowed.contains(&ins.opcode) => break,
                    _ => {}
                }
            }

            // If we got a name, rename the trait with it. Do not use trait.name =
            // ... because it will do some 💩 with multinames
            if name != 0 {
                t.rename(self.cpool, self.cpool.get_str(name)?.to_string())?;
            }
        }
        Ok(())
    }
    fn rename_readany(&mut self, classes: &Classes) -> Result<()> {
        let Some(index) = classes.varint_reader else {
            return Ok(()); // ignore
        };

        let varint_reader = &self.abc.classes[index];
        let mut read_varint = false;
        for t in &varint_reader.itraits {
            let Trait::Method(m) = t else {
                continue;
            };
            let method = self.abc.get_method(m.index)?;
            if method.params.is_empty()
                && method.local_count == 1
                && method.max_stack <= 2
                && method.init_scope_depth == method.max_scope_depth - 1
            {
                let instructions = method.parse()?;
                let mut prog = instructions.iter_prog();
                let getbuffer_op = Op::GetProperty(PropertyArg {
                    property: varint_reader.itraits[0].name(),
                });
                let call_buffer_method = [OpCode::GetProperty, OpCode::CallProperty];
                let prop = match prog.skip_until_match(&call_buffer_method)[..] {
                    [prop, Op::CallProperty(p)] if prop == &getbuffer_op => p.property,
                    _ => continue,
                };

                // instead of using `ByteArray.readBoolean()`, `ByteArray.readByte() != 0` is used
                // so we work around it
                let name = match self.cpool.qname(method.return_type) {
                    Some(name) if name == "Boolean" => "readBoolean".to_owned(),
                    _ => self.cpool.qname(prop).ok_or(anyhow!("Invalid property"))?,
                };
                t.rename(self.cpool, name)?;
            } else if !read_varint {
                read_varint = true;
                t.rename_str(self.cpool, "readVarInt")?;
            }
        }
        Ok(())
    }
    fn rename_interface_proxy(&mut self, classes: &Classes) -> Result<()> {
        let Some(index) = classes.interface_proxy else {
            return Ok(()); // ignore
        };

        let interface_proxy = &self.abc.classes[index];
        let ctor = self.abc.get_method(interface_proxy.iinit)?;
        interface_proxy.rename_str(self.cpool, "InterfaceProxy")?;

        let instructions = ctor.parse()?;
        let mut prog = instructions.iter_prog();
        while prog.has_next() {
            let key = match prog.skip_until_op(&OpCode::PushString) {
                Some(Op::PushString(p)) => p.value,
                _ => break,
            };
            let mn = match prog.skip_until_op(&OpCode::GetProperty) {
                Some(Op::GetProperty(p)) => p.property,
                _ => break,
            };
            let name = self.cpool.str(mn).unwrap_or_default();
            for prefix in ["method_", "name_", "const_"] {
                if name.rfind(prefix) == Some(0) {
                    match self.cpool.get_mn_mut(mn)? {
                        Multiname::QName(qname) | Multiname::QNameA(qname) => qname.name = key,
                        _ => {}
                    }
                    break;
                }
            }
        }
        Ok(())
    }

    fn get_known_name(&self, side: &PktNames, pkt_id: u16) -> String {
        let fallback = || PKT_NAMES.get(side, pkt_id);
        let name = self.packet_names.get(side, pkt_id);
        name.or_else(fallback).cloned().unwrap_or_default()
    }
    fn format_packet(&self, side: &PktNames, pkt_id: u16) -> String {
        let name = self.get_known_name(side, pkt_id);
        self.fmt.packets(side, pkt_id, name)
    }
    fn format_packet2(&self, side: &PktNames, categ_id: u8, pkt_id: u8) -> String {
        self.format_packet(side, ((categ_id as u16) << 8) | pkt_id as u16)
    }

    fn find_clientbound_packets(&mut self, classes: &Classes, ns: &NsNames) -> Result<()> {
        let Some(pkt_hdlr_idx) = classes.pkt_hdlr else {
            return Ok(()); //ignore
        };
        self.set_class_ns(pkt_hdlr_idx, ns.pkt)?;
        let pkt_hdlr = &self.abc.classes[pkt_hdlr_idx];
        let ctrait = pkt_hdlr
            .ctraits
            .iter()
            .find(|t| self.is_trait_packet_handler(t));

        let Some(ctrait) = ctrait else {
            bail!("Packet Handler method was not found.")
        };
        let trait_name = ctrait.name();
        pkt_hdlr.rename_str(self.cpool, "PacketHandler")?;
        ctrait.rename_str(self.cpool, "handle_packet")?;

        let method = self.get_method_from_trait(ctrait)?;
        let pkt_hdlr_name = self.abc.classes[pkt_hdlr_idx].name;
        let instructions = method.parse()?;
        let mut instructions1;
        let mut prog = instructions.iter_prog();
        let addr2idx: HashMap<_, _> = instructions
            .iter()
            .enumerate()
            .map(|(i, ins)| (ins.addr, i))
            .collect();

        while prog.has_next() {
            let (mut category, mut code) = (0, 0);
            if self.get_packet_code(&mut prog, &mut category, pkt_hdlr_name)? {
                let target = prog.get().targets[0];
                let mut found = false;
                if prog.next().is_some_and(|p| p.is(&OpCode::PushDouble)) {
                    prog.next();
                }

                while self.get_packet_code(&mut prog, &mut code, pkt_hdlr_name)? {
                    let codetarget = prog.get().targets[0];

                    // Handle the tribulle packets
                    if category == 0x3c && code == 0x03 {
                        found = self.find_clientbound_tribulle(&mut prog, ns)?;
                        break;
                    }

                    while prog.has_next() && !prog.is(&OpCode::ReturnVoid) {
                        if let Some(Op::FindPropStrict(p)) = prog.get_match(&NEW_CLASS_SEQ).first()
                        {
                            let (i, class) = match self.find_class_by_name(p.property) {
                                Some(cls) if !self.is_class_with_buffer_trait(cls.1) => cls,
                                _ => break,
                            };
                            let class_name = class.name;
                            self.set_class_ns(i, ns.cpkt)?;
                            self.cpool.rename_multiname(
                                class_name,
                                self.format_packet2(&PktNames::Clientbound, category, code),
                            )?;
                        }
                        prog.next();
                    }

                    // skip to the end of the if block
                    instructions1 = &instructions[addr2idx[&codetarget]..];
                    prog = instructions1.iter_prog();
                    found = true;
                    if prog.get().addr == target {
                        break;
                    }
                    if prog.is(&OpCode::PushDouble) {
                        prog.next();
                    }
                }

                if !found && prog.is(&SUB_HANDLER_SEQ) {
                    match (&prog.get().op, prog.peek(2)) {
                        (Op::GetLex(cls), Some(Op::GetLex(prop))) => {
                            if prop.property == pkt_hdlr_name {
                                if let Some(handler) = self.find_class_by_name(cls.property) {
                                    log::info!("Found sub handler {:?}", self.cpool.fqn(handler.1));
                                    self.find_clientbound_sub_packets(
                                        handler.0, trait_name, category, ns,
                                    )?;
                                }
                            }
                        }
                        _ => unreachable!(),
                    }
                }
                // go to the target to ensure a consistent state with the if-else block
                instructions1 = &instructions[addr2idx[&target]..];
                prog = instructions1.iter_prog();
                continue; // don't skip an instruciton
            }
            prog.next();
        }

        Ok(())
    }
    fn find_clientbound_sub_packets(
        &mut self,
        class: usize,
        trait_name: u32,
        category: u8,
        ns: &NsNames,
    ) -> Result<()> {
        let Some(ctrait) = self.find_ctrait_by_name(class, trait_name, true) else {
            bail!("Cannot find sub handler method.");
        };

        let instructions = self.get_method_from_trait(ctrait)?.parse()?;
        let mut instructions1;
        let mut prog = instructions.iter_prog();
        let addr2idx: HashMap<_, _> = instructions
            .iter()
            .enumerate()
            .map(|(i, ins)| (ins.addr, i))
            .collect();
        let pushdouble_seq = OpSeq([OpCode::GetLocal2, OpCode::PushDouble, OpCode::IfNe])
            | OpSeq([OpCode::PushDouble, OpCode::GetLocal2, OpCode::IfNe]);

        while prog.has_next() {
            prog.get_match(&pushdouble_seq);
            let (value, target) = match prog.skip_until_match(&pushdouble_seq)[..] {
                [Op::GetLocal2(), Op::PushDouble(p), Op::IfNe(t)]
                | [Op::PushDouble(p), Op::GetLocal2(), Op::IfNe(t)] => (p.value, t.target),
                _ => continue,
            };
            let code = *self.cpool.get_double(value)? as u8;

            while prog.has_next() && !prog.is(&OpCode::ReturnVoid) {
                if let Some(Op::FindPropStrict(p)) = prog.get_match(&NEW_CLASS_SEQ).first() {
                    let Some((i, _)) = self.find_class_by_name(p.property) else {
                        break;
                    };
                    let name = self.format_packet2(&PktNames::Clientbound, category, code);
                    self.abc.classes[i].rename(self.cpool, name)?;
                    self.set_class_ns(i, ns.cpkt)?;
                    break;
                }
                if prog.get().addr == target {
                    break;
                }
                prog.next();
            }
            instructions1 = &instructions[addr2idx[&target]..];
            prog = instructions1.iter_prog();
        }

        self.abc.classes[class].rename(self.cpool, self.fmt.subhandler(category))?;
        self.set_class_ns(class, ns.pkt)?;
        Ok(())
    }

    fn find_clientbound_tribulle(&mut self, prog: &mut InsIterator, ns: &NsNames) -> Result<bool> {
        let method = match prog.skip_until_match(&TRIBULLE_PKT_GETTER_SEQ)[..] {
            [Op::GetLex(lex), _, _, Op::CallProperty(call), _] => {
                let Some((class, _)) = self.find_class_by_name(lex.property) else {
                    return Ok(false);
                };
                let Some(ctrait) = self.find_ctrait_by_name(class, call.property, true) else {
                    return Ok(false);
                };
                // Make sure we have a method!
                let Ok(method) = self.get_method_from_trait(ctrait) else {
                    return Ok(false);
                };
                method
            }
            _ => return Ok(false),
        };

        // This method calls another one, which contains the code we want
        let instructions = method.parse()?;
        let mut prog = instructions.iter_prog();

        // First we have a getlex
        let Some(Op::GetLex(p)) = prog.skip_until_op(&OpCode::GetLex) else {
            return Ok(false);
        };
        let Some((mut klass, _)) = self.find_class_by_name(p.property) else {
            return Ok(false);
        };

        prog.next();
        // then we should have 2 getproperty
        while prog.has_next() {
            let Op::GetProperty(p) = &prog.get().op else {
                break;
            };
            // Make sure we get a slot trait with a specified type
            let slot = match self.find_trait(klass, p.property) {
                Some(Trait::Slot(slot)) if slot.slot_type != 0 => slot,
                _ => return Ok(false),
            };

            // Get the trait's type, so we can resolve later traits
            let Some(class) = self.find_class_by_name(slot.slot_type) else {
                return Ok(false);
            };
            klass = class.0;
            prog.next();
        }

        // followed by some args and a callproperty
        let Some(Op::CallProperty(p)) = prog.skip_until_op(&OpCode::CallProperty) else {
            return Ok(false);
        };

        // Get the class & method
        let name = p.property;
        let mut itrait = None;
        while itrait.is_none() {
            itrait = self.find_itrait_by_name(klass, name, false);
            if itrait.is_some() {
                break;
            }
            let Some(class) = self.find_class_by_name(self.abc.classes[klass].super_name) else {
                return Ok(false);
            };
            klass = class.0;
        }
        let Some(Trait::Method(m)) = itrait else {
            return Ok(false);
        };

        // found the magic method, we need to do the get_packet_code thing again!
        let method = self.abc.get_method(m.index)?;
        let return_type = method.return_type;
        let instructions = method.parse()?;
        let mut prog = instructions.iter_prog();

        // The same class has several interesting stuff
        self.find_serverbound_tribulle(klass, ns)?;

        // similar to get_packet_code, but much simpler
        // it's always `local2 == <pushdouble>` (or inversed)
        // so we only need to find the pushdouble
        while prog.has_next() {
            let Op::PushDouble(p) = &prog.get().op else {
                prog.next();
                continue;
            };

            let code = *self.cpool.get_double(p.value)? as u16;
            // find the next findpropstrict, that's the class we need to rename!
            let Some(Op::FindPropStrict(p)) = prog.skip_until_op(&OpCode::FindPropstrict) else {
                continue; // should we return false?
            };
            let Some((class, _)) = self.find_class_by_name(p.property) else {
                continue;
            };
            // rename it!
            let name = self.get_known_name(&PktNames::TribulleClientbound, code);
            self.abc.classes[class].rename(self.cpool, name)?;
            self.set_class_ns(class, ns.tcpkt)?;
        }

        if let Some((klass, _)) = self.find_class_by_name(return_type) {
            self.abc.classes[klass].rename_str(self.cpool, "TCPacketBase")?;
            self.set_class_ns(klass, ns.tpkt)?;
        }
        Ok(true)
    }
    fn find_serverbound_tribulle(&mut self, klass: usize, ns: &NsNames) -> Result<()> {
        // First we can get the Tribulle aka Community Platform version
        let iinit = self.abc.classes[klass].iinit;
        let instructions = self.abc.get_method(iinit)?.parse()?;
        let mut prog = instructions.iter_prog();

        // Skip it if we don't find it, that's not the end of the world
        if let Some(Op::PushString(p)) = prog.skip_until_op(&OpCode::PushString) {
            let version = self.cpool.get_str(p.value)?;
            log::info!("Found Tribulle v{version}");
        } else {
            log::warn!("Tribulle version not found.");
        }

        // this class has a method called "getIdPaquet" which takes one param,
        // and return its corresponding id. The function check the param's type using `istypelate`
        // So we can get the class from its id and rename it.
        let Some(itrait) = self.abc.classes[klass].itraits.iter().find(|itrait| {
            match self.get_method_from_trait(itrait) {
                Ok(meth) if meth.params.len() == 1 => {
                    self.cpool.qname(meth.return_type) == Some("int".to_owned())
                }
                _ => false,
            }
        }) else {
            return Ok(());
        };
        itrait.rename_str(self.cpool, "getPacketId")?;
        let Ok(method) = self.get_method_from_trait(itrait) else {
            return Ok(());
        };

        let instructions = method.parse()?;
        let mut prog = instructions.iter_prog();

        let mut addr2id = HashMap::new();
        let mut index2name = HashMap::new();
        let mut addr = prog
            .skip_until(&TRIBULLE_PKT_RETURN_ID_SEQ)
            .map_or(0, |p| p.get().addr);

        while let Some(Op::PushDouble(p)) = prog.get_match(&TRIBULLE_PKT_RETURN_ID_SEQ).get(1) {
            addr2id.insert(addr, *self.cpool.get_double(p.value)? as u16);
            addr = prog.get().addr;
        }

        // find the getlex's and the index used in the lookupswitch
        while prog.has_next() {
            let Some(Op::GetLex(getlex)) = prog.skip_until_op(&OpCode::GetLex) else {
                break;
            };
            let Some(Op::PushByte(p)) = prog.skip_until_op(&OpCode::PushByte) else {
                break;
            };
            index2name.insert(p.value, getlex.property);
            prog.next();
            if !prog.is(&[OpCode::Jump, OpCode::GetLocal1]) {
                break;
            }
        }
        if prog.skip_until(&OpCode::LookupSwitch).is_none() {
            return Ok(());
        }
        let target_count: u8 = prog.get().targets.len().try_into()?;
        for (i, name) in index2name {
            if i >= target_count {
                continue;
            }
            let Some((cls, _)) = self.find_class_by_name(name) else {
                continue;
            };
            let Some(&code) = addr2id.get(&prog.get().targets[i as usize + 1]) else {
                continue;
            };
            let name = self.format_packet(&PktNames::TribulleServerbound, code);
            self.abc.classes[cls].rename(self.cpool, name)?;
            self.set_class_ns(cls, ns.tspkt)?;
        }
        Ok(())
    }

    fn get_packet_code(
        &self,
        prog: &mut InsIterator<'_>,
        code: &mut u8,
        pkt_hdlr_name: u32,
    ) -> Result<bool> {
        let advance = |prog: &mut InsIterator<'_>, offset| match prog.peek(offset) {
            Some(Op::IfNe(_)) => {
                (0..offset).for_each(|_| {
                    prog.next();
                });
                true
            }
            _ => false,
        };
        match &prog.get().op {
            Op::GetLex(p) if p.property == pkt_hdlr_name => {}
            _ => return Ok(false),
        }
        if !matches!(prog.peek(1), Some(Op::GetProperty(_))) {
            return Ok(false);
        }
        if let Some(Op::PushDouble(p)) = prog.peek(2) {
            *code = *self.cpool.get_double(p.value)? as u8;
            return Ok(advance(prog, 3));
        }

        // the pushdouble could be right before
        if let Some(Op::PushDouble(p)) = &prog.prev_op() {
            *code = *self.cpool.get_double(p.value)? as u8;
            return Ok(advance(prog, 2));
        }

        Ok(false)
    }

    fn find_class_by_name(&self, name: u32) -> Option<(usize, &Class)> {
        if name == 0 {
            return None;
        }
        self.abc
            .classes
            .iter()
            .find_position(|cls| cls.name == name)
    }

    fn is_buffer_trait(&self, itrait: &Trait) -> bool {
        matches!(itrait, Trait::Slot(slot) if Some(slot.slot_type) == self.byte_array
            && self.cpool.qname(slot.name) == Some("buffer".to_owned()))
    }
    fn is_class_with_buffer_trait(&self, class: &Class) -> bool {
        !class.itraits.is_empty() && self.is_buffer_trait(&class.itraits[0])
    }

    fn get_method_from_trait(&self, ctrait: &Trait) -> Result<&Method> {
        let Trait::Method(m) = ctrait else {
            bail!("Invalid trait, expected Trait::Method, got {ctrait:?}");
        };
        Ok(self.abc.get_method(m.index)?)
    }

    fn find_ctrait_by_name(&self, class: usize, name: u32, check_super: bool) -> Option<&Trait> {
        let klass = &self.abc.classes[class];
        let ctrait = klass.ctraits.iter().find(|t| t.name() == name);
        if ctrait.is_some() || !check_super || klass.super_name == 0 {
            return ctrait;
        }
        // Check the prototype chain
        // TODO: unroll it to prevent any recursive issue?
        self.find_class_by_name(klass.super_name)
            .and_then(|(i, _)| self.find_ctrait_by_name(i, name, check_super))
    }
    fn find_itrait_by_name(&self, class: usize, name: u32, check_super: bool) -> Option<&Trait> {
        let klass = &self.abc.classes[class];
        let itrait = klass.itraits.iter().find(|t| t.name() == name);
        if itrait.is_some() || !check_super || klass.super_name == 0 {
            return itrait;
        }
        // Check the prototype chain
        // TODO: unroll it to prevent any recursive issue?
        self.find_class_by_name(klass.super_name)
            .and_then(|(i, _)| self.find_itrait_by_name(i, name, check_super))
    }
    fn find_trait(&self, class: usize, name: u32) -> Option<&Trait> {
        let t = self.find_ctrait_by_name(class, name, false);
        t.or_else(|| self.find_itrait_by_name(class, name, false))
    }
}

impl Detfm<'_> {
    /// Check if the given class is the WrapClass with the following attributes
    ///  - All traits are class methods
    ///  - All methods takes a single parameter and returns it
    ///  - The param and return types are the same
    ///  - No instance traits
    fn is_wrap_class(&self, class: &Class) -> bool {
        if !class.itraits.is_empty() || class.ctraits.is_empty() {
            return false;
        }
        for ctrait in &class.ctraits {
            match self.get_method_from_trait(ctrait) {
                Ok(method) if method.params == [method.return_type] => {}
                _ => return false,
            };
        }
        true
    }

    fn is_slot_class(&self, class: &Class) -> bool {
        if !class.itraits.is_empty() || class.ctraits.len() < 100 {
            return false;
        }

        for ctrait in &class.ctraits {
            match ctrait {
                Trait::Slot(slot) if slot.attr == TraitAttr::empty() => {}
                Trait::Method(method) => {
                    if !method.attr.contains(TraitAttr::FINAL) {
                        return false;
                    }

                    let method = self.abc.get_method(method.index).ok();
                    match method.and_then(|m| self.cpool.qname(m.return_type)) {
                        Some(name) if name == "int" || name == "Number" => {}
                        _ => return false,
                    }
                }
                _ => return false,
            }
        }
        true
    }

    fn is_serverbound_pkt(&self, class: &Class) -> bool {
        if !class.is_sealed() || !class.is_protected() {
            return false;
        }
        matches!(class.itraits.first(), Some(Trait::Slot(slot)) if Some(slot.slot_type) == self.byte_array)
    }

    fn is_clientbound_pkt(&self, class: &Class) -> bool {
        let isize = class.itraits.len();
        let csize = class.ctraits.len();
        if csize == 0 || csize > 9 || !(4..=9).contains(&isize) {
            return false;
        }

        match &class.itraits[2] {
            Trait::Slot(slot) => Some(slot.slot_type) == self.byte_array,
            _ => false,
        }
    }

    fn is_packet_handler(&self, class: &Class) -> bool {
        let is_handler = |t| self.is_trait_packet_handler(t);
        class.itraits.is_empty() && class.ctraits.iter().any(is_handler)
    }
    fn is_trait_packet_handler(&self, ctrait: &Trait) -> bool {
        self.get_method_from_trait(ctrait).is_ok_and(|method| {
            method.local_count >= 200
                && method.max_stack >= 30
                && method.params == [self.byte_array.unwrap_or(0)]
        })
    }

    fn is_varint_reader(&self, class: &Class) -> bool {
        let bytearray = self.byte_array.unwrap_or(0);
        let Some(iinit) = self.abc.get_method(class.iinit).ok() else {
            return false;
        };
        matches!(class.itraits.first(), Some(Trait::Slot(slot)) if slot.slot_type == bytearray)
            && iinit.params == [bytearray]
    }

    fn is_interface_proxy(&self, klass: &Class) -> bool {
        let Ok(ctor) = self.abc.get_method(klass.iinit) else {
            return false;
        };
        klass.ctraits.is_empty()
            && klass.itraits.is_empty()
            && klass.protected_ns != 0
            && ctor.params == [self.abc.classes[0].name]
    }
}
