#[derive(Clone, Copy, Debug, Default, Eq, PartialEq, Hash, serde::Serialize)]
pub enum FilAdcInstance { #[default] #[serde(rename="ADC1")] Adc1, #[serde(rename="ADC2")] Adc2, #[serde(rename="ADC3")] Adc3, #[serde(rename="ADC4")] Adc4 }
impl FilAdcInstance { pub const ALL:[Self;4]=[Self::Adc1,Self::Adc2,Self::Adc3,Self::Adc4]; pub const fn as_str(self)-> &'static str {match self{Self::Adc1=>"ADC1",Self::Adc2=>"ADC2",Self::Adc3=>"ADC3",Self::Adc4=>"ADC4"}} pub fn parse(s:&str)->Option<Self>{Self::ALL.into_iter().find(|v|v.as_str()==s)} }
impl std::fmt::Display for FilAdcInstance {fn fmt(&self,f:&mut std::fmt::Formatter<'_>)->std::fmt::Result{f.write_str(self.as_str())}}
impl<'de> serde::Deserialize<'de> for FilAdcInstance {fn deserialize<D:serde::Deserializer<'de>>(d:D)->Result<Self,D::Error>{let s=<Option<String> as serde::Deserialize>::deserialize(d)?;Ok(s.as_deref().and_then(Self::parse).unwrap_or_default())}}
#[derive(Clone,Copy,Debug,Eq,PartialEq,Hash)]
pub enum FilGpioPort{GpioA,GpioB,GpioC,GpioD,GpioE,GpioF,GpioG}
impl FilGpioPort{pub const ALL:[Self;7]=[Self::GpioA,Self::GpioB,Self::GpioC,Self::GpioD,Self::GpioE,Self::GpioF,Self::GpioG];pub const fn as_str(self)-> &'static str{match self{Self::GpioA=>"GPIOA",Self::GpioB=>"GPIOB",Self::GpioC=>"GPIOC",Self::GpioD=>"GPIOD",Self::GpioE=>"GPIOE",Self::GpioF=>"GPIOF",Self::GpioG=>"GPIOG"}}pub fn parse(s:&str)->Option<Self>{Self::ALL.into_iter().find(|v|v.as_str()==s)}}
impl std::fmt::Display for FilGpioPort{fn fmt(&self,f:&mut std::fmt::Formatter<'_>)->std::fmt::Result{f.write_str(self.as_str())}}
