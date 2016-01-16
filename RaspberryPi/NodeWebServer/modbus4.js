var FC = require('./modbus-stack').FUNCTION_CODES;
var client = require('./modbus-stack/client').createClient(502);

var http = require("http"),
    url = require("url"),
    path = require("path"),
    fs = require("fs"),
    port = process.argv[2] || 80;


var system_state = 
{
   control_active : false,
   supply_power_on : false,
   speed_demand : 0,
   speed_actual : 0,
   speed_actual_volts : 0,
   speed_demand_volts : 0,
   supply_power_volts : 0,
   speed_actual_filter_COF : 0,
   supply_power_filter_COF : 0,
   control_deadband_volts : 0,
   control_slowband_volts : 0
};
   

var holding_registers = [];

setInterval(service_modbus_queue, 100);

var modbus_queue_len = 0;
var modbus_queue = [];
var modbus_busy = false;
function service_modbus_queue()
{
   if ((!modbus_busy) && (modbus_queue_len > 0))
   {
      modbus_queue[ modbus_queue_len - 1 ]();
      modbus_queue_len--;
   }
}

function write_modbus_queue(req_function)
{
   modbus_queue[ modbus_queue_len ] = req_function;
   modbus_queue_len++;
}

setInterval(read_registers, 1000);

function read_registers()
{
   write_modbus_queue(function() {
      client.request(FC.READ_HOLDING_REGISTERS, 0, 10, function(err, response) {
         modbus_busy = false;
         if (err) throw err;
         console.log(response);
         for(k = 0; k < 10; k++)
         {
            holding_registers[ k ] = response[ k ];
         }
         system_state.control_active = ((holding_registers[ 1 ] & 0x0002) != 0);
         system_state.speed_demand_volts = holding_registers[ 2 ] * 5.0 / 1024;
         system_state.speed_actual_volts = holding_registers[ 3 ] * 5.0 / 1024;
         system_state.control_deadband_volts = holding_registers[ 4 ] * 5.0 / 1024;
         system_state.control_slowband_volts = holding_registers[ 5 ] * 5.0 / 1024;
         system_state.speed_actual_filter_COF = Math.log(holding_registers[ 6 ] / 32768.0) * 100 / (-2.0 * Math.PI);
         system_state.supply_power_volts = holding_registers[ 7 ] * 5.0 / 1024;
         system_state.supply_power_filter_COF = Math.log(holding_registers[ 8 ] / 32768.0) * 100 / (-2.0 * Math.PI);


console.log(holding_registers);
     });
   });
}



http.createServer(function(request, response) {

  var uri = url.parse(request.url).pathname
    , filename = path.join(process.cwd(), uri);

  console.log(uri);
  console.log(request.method);

  if (request.method === "PUT")
  {
     var body = "";
     request.on('data', function(chunk) 
        {
           //console.log("Received body data:");
           //console.log(chunk.toString());
           body += chunk.toString();
        });
    
     request.on('end', function() 
        {
           var success = false;
           var address = 0;
           var value = 0;
           if (uri === "/system_state/speed_demand_volts")
           {              
              address = 2;
              value = JSON.parse(body);
              if ((value) && (value >= 0.0) && (value <= 5.0))
              {
                 value *= 1024.0 / 5.0;
                 value = Math.round(value);
                 value = Math.max(value, 0);
                 value = Math.min(value, 1023);
                 success = true;
              }
           }
           else if (uri === "/system_state/speed_actual_filter_COF")
           {
              address = 6;
              value = JSON.parse(body);
              if ((value) && (value >= 0.0) && (value <= 50.0))
              {
                 value *= -2.0 * Math.PI / 100.0;
                 value = Math.exp(value) * 32768.0;
                 value = Math.round(value);
                 value = Math.max(0, value);
                 value = Math.min(0x8000, value);
                 success = true;
              }
           }
              

           if (success)
           {
              write_modbus_queue(function() {
                 client.request(FC.WRITE_SINGLE_REGISTER, address, value, function(err, MB_response) {
                    modbus_busy = false;
                   if (err) throw err;
                   console.log(MB_response);
                   });
                });

              response.writeHead(204);
              response.end();
              return;
           }
           else
           {
              response.writeHead(400, {"Content-Type": "text/plain"});
              response.write("400 Invalid Resourse [" + uri + "]\n");
              response.end();
              return;
           }
        });
  }
  else if (request.method === "GET")
  {
     if (uri === "/system_state")
     {
        response.writeHead(200, {"Content-Type": "application/json"});
        response.write(JSON.stringify(system_state));
        response.end();
        return;
     }

     fs.exists(filename, function(exists) 
        {
           if(!exists) 
           {
              response.writeHead(404, {"Content-Type": "text/plain"});
              response.write("404 Not Found\n");
              response.end();
              return;
           }

           if (fs.statSync(filename).isDirectory()) filename += '/index.html';

           fs.readFile(filename, "binary", function(err, file) 
              {
                 if(err) 
                 {        
                    response.writeHead(500, {"Content-Type": "text/plain"});
                    response.write(err + "\n");
                    response.end();
                    return;
                 }

                 response.writeHead(200);
                 response.write(file, "binary");
                 response.end();
              });
        });
  }
}).listen(parseInt(port, 10));

console.log("Static file server running at\n  => http://localhost:" + port + "/\nCTRL + C to shutdown");

