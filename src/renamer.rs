use crate::{
    fmt,
    rename::{PoolRenamer, Rename, RenameSuper},
};
use anyhow::Result;
use rabc::{
    abc::{Class, ConstantPool, Exception, Method, Trait},
    Abc,
};

macro_rules! counter {
    ($name:ident) => {
        fn $name(&mut self) -> u32 {
            let value = self.$name;
            self.$name += 1;
            value
        }
    };
}

#[derive(Debug, Default)]
struct Counters {
    classes: u32,
    consts: u32,
    functions: u32,
    _names: u32,
    vars: u32,
    methods: u32,
}
impl Counters {
    counter!(classes);
    counter!(consts);
    counter!(functions);
    counter!(_names);
    counter!(vars);
    counter!(methods);
}

#[derive(Debug, Default)]
pub struct Renamer {
    counters: Counters,
}

impl Renamer {
    pub fn invalid(name: &str) -> bool {
        name.chars().any(|c| !c.is_alphabetic())
    }
    fn invalid_multiname(cpool: &ConstantPool, index: u32) -> Result<bool> {
        Ok(Self::invalid(cpool.get_str(cpool.get_mn_name(index)?)?))
    }

    pub fn rename_all(&mut self, abc: &mut Abc, cpool: &mut ConstantPool) -> Result<()> {
        for class in &abc.classes {
            self.rename_invalid_class(cpool, class)?;
        }

        for method in &abc.methods {
            self.rename_invalid_method(cpool, method)?;
        }
        Ok(())
    }
    fn rename_invalid_class(&mut self, cpool: &mut ConstantPool, class: &Class) -> Result<()> {
        if class.name != 0 && Self::invalid_multiname(cpool, class.name)? {
            class.rename(cpool, fmt::classes(self.counters.classes()))?;
        }
        if class.super_name != 0 && Self::invalid_multiname(cpool, class.super_name)? {
            class.rename_super(cpool, fmt::classes(self.counters.classes()))?;
        }

        for cls_trait in class.ctraits.iter().chain(class.itraits.iter()) {
            self.rename_invalid_trait(cpool, cls_trait)?;
        }
        Ok(())
    }

    fn rename_invalid_trait(&mut self, cpool: &mut ConstantPool, cls_trait: &Trait) -> Result<()> {
        let name = cls_trait.name();
        if name != 0 && Self::invalid_multiname(cpool, name)? {
            let name = match cls_trait {
                Trait::Const(_) => fmt::consts(self.counters.consts()),
                Trait::Method(_) => fmt::methods(self.counters.methods()),
                Trait::Function(_) => fmt::functions(self.counters.functions()),
                _ => fmt::vars(self.counters.vars()),
            };
            cls_trait.rename(cpool, name)?;
        }
        Ok(())
    }

    fn rename_invalid_method(&mut self, cpool: &mut ConstantPool, method: &Method) -> Result<()> {
        for (i, err) in method.exceptions.iter().enumerate() {
            Self::rename_invalid_exception(cpool, err, i as u32)?;
        }

        for mtrait in &method.traits {
            self.rename_invalid_trait(cpool, mtrait)?;
        }
        Ok(())
    }

    fn rename_invalid_exception(
        cpool: &mut ConstantPool,
        exception: &Exception,
        counter: u32,
    ) -> Result<()> {
        if Self::invalid_multiname(cpool, exception.var_name)? {
            exception.rename(cpool, fmt::errors(counter))?;
        }
        Ok(())
    }
}
