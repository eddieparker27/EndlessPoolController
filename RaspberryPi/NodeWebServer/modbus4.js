var FC = require('./modbus-stack').FUNCTION_CODES;

var client = require('./modbus-stack/client').createClient(502);

var speed_demand = 1;

setTimeout(read_registers, 1000);

function read_registers()
{
   client.request(FC.READ_HOLDING_REGISTERS, 0, 10, function(err, response) {
      if (err) throw err;
      console.log(response);
      client.request(FC.WRITE_SINGLE_REGISTER, 2, speed_demand++, function(err, response) {
              if (err) throw err;
              console.log(response);
              //client.end();
              setTimeout(read_registers, 1000);
              });
     });
}

