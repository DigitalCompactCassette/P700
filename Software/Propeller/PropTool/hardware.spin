''***************************************************************************
''* Pin assignments and other global constants
''* Copyright (C) 2019 Jac Goudsmit
''*
''* TERMS OF USE: MIT License. See bottom of file.                                                            
''***************************************************************************
''

CON

  #0

  pin_0  'pin_MSDDO                     ' SD card data out                        
  pin_1  'pin_MSDCK                     ' SD card clock                                                              
  pin_2  'pin_MSDDI                     ' SD card data in                       
  pin_3  'pin_MSDCS                     ' SD card chip select 

  ' 4
  pin_L3REF                     ' L3 bus reference                        
  pin_L3MODE                    ' L3 bus mode 
  pin_L3DATA                    ' L3 bus data
  pin_L3CLK                     ' L3 bus clock

  ' 8
  pin_8                         ' Reserved                       
  pin_9                         ' Reserved                                                
  pin_10                        ' Reserved                        
  pin_11                        ' Reserved                        

  ' 12
  pin_12 'pin_I2SCK                     ' Sub-band I2S clock
  pin_13 'pin_I2SWS                     ' Sub-band I2S word select
  pin_14 'pin_I2SDRP                    ' Sub-band I2S serial data (to/from DRP)
  pin_15 'pin_I2SSFC                    ' Sub-band I2S serial data (to/from SFC)

  ' 16
  pin_FPHOLD                    ' Front panel hold signal 
  pin_FPCLK                     ' Front panel synchronous serial clock                         
  pin_FPSLIN                    ' Front panel slave in
  pin_FPSLOUT                   ' Front panel slave out                        

  ' 20
  pin_DECKRX                    ' Deck control RX
  pin_DECKTX                    ' Deck control TX                                                 
  pin_PWRDWN                    ' Power Down
  pin_NDCCRES                   ' DCC Reset

  ' 24  
  pin_24 'pin_NINTCTRL                  ' Intercept control lines                        
  pin_25 'pin_NINTI2S                   ' Intercept subband I2S                                                
  pin_26 'pin_FPSLIN2                   ' Front panel slave input  to MCU                        
  pin_27 'pin_FPSLOUT2                  ' Front panel slave output to MCU                        

  ' 28   
  pin_SCL                       ' I2C clock                                      
  pin_SDA                       ' I2C data
  pin_TX                        ' Serial transmit                        
  pin_RX                        ' Serial receive


PUB dummy
{{ The module won't compile without at least one public function }}

CON     
''***************************************************************************
''* MIT LICENSE
''*
''* Permission is hereby granted, free of charge, to any person obtaining a
''* copy of this software and associated documentation files (the
''* "Software"), to deal in the Software without restriction, including
''* without limitation the rights to use, copy, modify, merge, publish,
''* distribute, sublicense, and/or sell copies of the Software, and to permit
''* persons to whom the Software is furnished to do so, subject to the
''* following conditions:
''*
''* The above copyright notice and this permission notice shall be included
''* in all copies or substantial portions of the Software.
''*
''* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
''* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
''* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
''* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
''* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT
''* OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
''* THE USE OR OTHER DEALINGS IN THE SOFTWARE.
''***************************************************************************