#include "ModbusSlave.h"

ModbusSlave::ModbusSlave() 
{

}

bool 
ModbusSlave::setSlaveId(byte slaveId)
{
   _slaveId = slaveId;
   return true;
}

byte 
ModbusSlave::getSlaveId() 
{
   return _slaveId;
}

bool 
ModbusSlave::config(HardwareSerial* port, long baud, unsigned int format) 
{
   this->_port = port;
   (*port).begin(baud, format);

   delay(2000);

   return true;
}

bool ModbusSlave::receive(byte* frame) 
{
   //first byte of frame = address
   byte address = frame[0];
   //Last two bytes = crc
   unsigned int crc = ((frame[_len - 2] << 8) | frame[_len - 1]);

   //Slave Check
   if (address != 0xFF && address != this->getSlaveId()) 
   {
      return false;
   }

   //CRC Check
   if (crc != this->calcCrc(_frame[0], _frame+1, _len-3)) 
   {
      return false;
   }

   //PDU starts after first byte
   //framesize PDU = framesize - address(1) - crc(2)
   this->receivePDU(frame+1);
   //No reply to Broadcasts
   if (address == 0xFF) 
   {
     _reply = MB_REPLY_OFF;
   }
   return true;
}

bool ModbusSlave::send(byte* frame) 
{
  (*_port).write(frame, _len);

  (*_port).flush();
}

void ModbusSlave::receivePDU(byte* frame)
{
  byte fcode  = frame[0];
  word field1 = (word)frame[1] << 8 | (word)frame[2];
  word field2 = (word)frame[3] << 8 | (word)frame[4];

  switch (fcode)
  {
  case MB_FC_WRITE_REG:
    //field1 = reg, field2 = value
    this->writeSingleRegister(field1, field2);
    break;

  case MB_FC_READ_REGS:
    //field1 = startreg, field2 = numregs
    this->readRegisters(field1, field2);
    break;

  case MB_FC_WRITE_REGS:
    //field1 = startreg, field2 = status
    this->writeMultipleRegisters(frame,field1, field2, frame[5]);
    break;

  default:
    this->exceptionResponse(fcode, MB_EX_ILLEGAL_FUNCTION);
  }
}

void ModbusSlave::exceptionResponse(byte fcode, byte excode) 
{
  //Clean frame buffer
  _len = 2;
  _frame[0] = fcode + 0x80;
  _frame[1] = excode;

  _reply = MB_REPLY_NORMAL;
}

void ModbusSlave::readRegisters(word startreg, word numregs) 
{
  //Check value (numregs)
  if (numregs < 0x0001 || numregs > 0x007D)
  {
    this->exceptionResponse(MB_FC_READ_REGS, MB_EX_ILLEGAL_VALUE);
    return;
  }

  //Check Address
  if (numregs > NUM_REGISTERS) 
  {
    this->exceptionResponse(MB_FC_READ_REGS, MB_EX_ILLEGAL_ADDRESS);
    return;
  }


  //Clean frame buffer
  _len = 0;

  //calculate the query reply message length
  //for each register queried add 2 bytes
  _len = 2 + numregs * 2;

  _frame[0] = MB_FC_READ_REGS;
  _frame[1] = _len - 2;   //byte count

  word val;
  word i = 0;
  while(numregs--) 
  {
    //retrieve the value from the register bank for the current register
    val = this->registers[startreg + i];
    //write the high byte of the register value
    _frame[2 + i * 2]  = val >> 8;
    //write the low byte of the register value
    _frame[3 + i * 2] = val & 0xFF;
    i++;
  }

  _reply = MB_REPLY_NORMAL;
}

void ModbusSlave::writeSingleRegister(word reg, word value) 
{
  //No necessary verify illegal value (EX_ILLEGAL_VALUE) - because using word (0x0000 - 0x0FFFF)
  //Check Address and execute (reg exists?)
  if (reg > (NUM_REGISTERS - 1)) 
  {
    this->exceptionResponse(MB_FC_READ_REGS, MB_EX_ILLEGAL_ADDRESS);
    return;
  }
  _reply = MB_REPLY_ECHO;
}

void ModbusSlave::writeMultipleRegisters(byte* frame, word startreg, word numoutputs, byte bytecount)
{
  //Check value
  if (numoutputs < 0x0001 || numoutputs > 0x007B || bytecount != 2 * numoutputs)
  {
    this->exceptionResponse(MB_FC_WRITE_REGS, MB_EX_ILLEGAL_VALUE);
    return;
  }

  //Check Address (startreg...startreg + numregs)
  if (startreg < 
  for (int k = 0; k < numoutputs; k++)
  {
        if (!this->searchRegister(startreg + 40001 + k)) {
            this->exceptionResponse(MB_FC_WRITE_REGS, MB_EX_ILLEGAL_ADDRESS);
            return;
        }
    }

    //Clean frame buffer
    free(_frame);
	_len = 5;
    _frame = (byte *) malloc(_len);
    if (!_frame) {
        this->exceptionResponse(MB_FC_WRITE_REGS, MB_EX_SLAVE_FAILURE);
        return;
    }

    _frame[0] = MB_FC_WRITE_REGS;
    _frame[1] = startreg >> 8;
    _frame[2] = startreg & 0x00FF;
    _frame[3] = numoutputs >> 8;
    _frame[4] = numoutputs & 0x00FF;

    word val;
    word i = 0;
	while(numoutputs--) {
        val = (word)frame[6+i*2] << 8 | (word)frame[7+i*2];
        this->Hreg(startreg + i, val);
        i++;
	}

    _reply = MB_REPLY_NORMAL;
}*/



word 
ModbusSlave::calcCrc(byte address, byte* pduFrame, byte pduLen) 
{
   byte CRCHi = 0xFF, CRCLo = 0x0FF, Index;
   
   Index = CRCHi ^ address;
   CRCHi = CRCLo ^ _auchCRCHi[Index];
   CRCLo = _auchCRCLo[Index];

   while (pduLen--) 
   {
      Index = CRCHi ^ *pduFrame++;
      CRCHi = CRCLo ^ _auchCRCHi[Index];
      CRCLo = _auchCRCLo[Index];
   }

   return (CRCHi << 8) | CRCLo;
}
