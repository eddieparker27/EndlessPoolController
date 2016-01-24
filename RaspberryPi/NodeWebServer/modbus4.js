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
   speed_actual_register : 0,
   speed_demand_register : 0,
   supply_power_volts : 0,
   speed_actual_filter_COF : 0.25, //Hz
   supply_power_filter_COF : 0.25, //Hz
   control_deadband_volts : 0.03, //Volts
   control_timeconst : 10000, // Milliseconds
   radio_tx_interval : 0 // Milliseconds
};

var holding_registers = [];
var HOLDING_REGISTER_ADDRESS =
{
    control : 0,
    response : 1,
    speed_demand : 2,
    speed_actual : 3,
    control_deadband : 4,
    control_timeconst : 5,
    speed_actual_filter_COF : 6,
    supply_power : 7,
    supply_power_filter_COF : 8,
    radio_tx_interval : 9
}

var CONTROL_ACTIVE_BIT = 0x0002;

setInterval(service_modbus_queue, 100);

var modbus_queue_len = 0;
var modbus_queue = [];
var modbus_busy = false;
function service_modbus_queue()
{
   if ((!modbus_busy) && (modbus_queue_len > 0))
   {
      modbus_busy = true;
      modbus_queue[ modbus_queue_len - 1 ]();
      modbus_queue_len--;
   }
}

function write_modbus_queue(req_function)
{
   modbus_queue[ modbus_queue_len ] = req_function;
   modbus_queue_len++;
}

var workouts = [];
var num_workouts = 0;
var active_workout = null;
var active_workout_index = -1;
var last_time = null;

function initialiseSystem()
{
   /*
   * Read in any workouts
   */
   files = fs.readdirSync("workouts");
   var len = files.length;
   for(var i = 0; i < len; i++)
   {
      var pathname = "workouts/" + files[ i ];
      console.log("Reading " + pathname);
      filedata = fs.readFileSync(pathname);
      var wo = JSON.parse(filedata);
      var parse_ok = false;
      if ((typeof(wo.name) !== 'undefined') &&
          (typeof(wo.description) !== 'undefined') &&
          (typeof(wo.sections) !== 'undefined'))
      {
         console.log("name, description and sections exist")
         parse_ok = true;
         var sec_len = wo.sections.length;
         for(var j = 0; j < sec_len; j++)
         {
            console.log("Checking section " + j);
            console.log(JSON.stringify(wo.sections[j]));
            if ((typeof(wo.sections[ j ].speed) === 'undefined') ||
                (typeof(wo.sections[ j ].duration) === 'undefined'))
            {
               console.log("Section " + j + " failed parsing");
               parse_ok = false;
               break;
            }
         }
         if (parse_ok)
         {
            console.log("File parsed okay. Adding");
            workouts[ num_workouts++ ] = wo;
         }
      }
   }

   /*
   * Initialise control parameters
   */
   setValue("speed_actual_filter_COF", system_state.speed_actual_filter_COF);
   setValue("supply_power_filter_COF", system_state.supply_power_filter_COF);
   setValue("control_deadband_volts", system_state.control_deadband_volts);
   setValue("control_timeconst", system_state.control_timeconst);
}

initialiseSystem();
setInterval(read_registers, 1000);

function set_active_workout_index(index)
{
   if ((index >= 0) && (index < num_workouts))
   {
      console.log("Active workout index = " + index);
      active_workout_index = index;
      active_workout = workouts[ active_workout_index ];
      active_workout.mode = 'OFF';
      active_workout.runtime = 0;
      var len = active_workout.sections.length;
      active_workout.cumtime = 0;
      for(var i = 0; i < len; i++)
      {
         active_workout.sections[ i ].cumtime = active_workout.cumtime;
         active_workout.cumtime += active_workout.sections[ i ].duration;
      }
   }
}

function read_registers()
{
   write_modbus_queue(function() {
      client.request(FC.READ_HOLDING_REGISTERS, 0, 10, function(err, response) {
         modbus_busy = false;
         if (err) throw err;
         for(k = 0; k < 10; k++)
         {
            holding_registers[ k ] = response[ k ];
         }
         system_state.control_active = ((holding_registers[HOLDING_REGISTER_ADDRESS.response] & CONTROL_ACTIVE_BIT) != 0);
         system_state.speed_demand_register = holding_registers[HOLDING_REGISTER_ADDRESS.speed_demand];
         system_state.speed_actual_register = holding_registers[HOLDING_REGISTER_ADDRESS.speed_actual];
         system_state.speed_demand_volts = system_state.speed_demand_register * 5.0 / 1024;
         system_state.speed_actual_volts = system_state.speed_actual_register * 5.0 / 1024;
         system_state.speed_demand = ((Math.min(Math.max(system_state.speed_demand_volts, 1.25), 3.05) - 1.25) * 30) + 1;
         system_state.speed_actual = ((Math.min(Math.max(system_state.speed_actual_volts, 1.25), 3.05) - 1.25) * 30) + 1;
         system_state.control_deadband_volts = holding_registers[HOLDING_REGISTER_ADDRESS.control_deadband] * 5.0 / 1024;
         system_state.control_timeconst = holding_registers[HOLDING_REGISTER_ADDRESS.control_timeconst];
         system_state.speed_actual_filter_COF = Math.log(holding_registers[ HOLDING_REGISTER_ADDRESS.speed_actual_filter_COF ] / 32768.0) * 100 / (-2.0 * Math.PI);
         system_state.supply_power_volts = holding_registers[HOLDING_REGISTER_ADDRESS.supply_power] * 5.0 / 1024;
         system_state.supply_power_on = (system_state.supply_power_volts > 2.0);
         system_state.supply_power_filter_COF = Math.log(holding_registers[HOLDING_REGISTER_ADDRESS.supply_power_filter_COF] / 32768.0) * 100 / (-2.0 * Math.PI);
         system_state.radio_tx_interval = holding_registers[HOLDING_REGISTER_ADDRESS.radio_tx_interval];

         var d = new Date();
         var time = d.getTime();
         var elapsed_ms = 0;
         if (last_time)
         {
            elapsed_ms = (time - last_time);
         }
         last_time = time;
         
         if (active_workout)
         {
            if ((active_workout.mode === 'ON') ||
                ((system_state.supply_power_on) && (active_workout.mode === 'AUTO')))
            {
               if (active_workout.runtime < active_workout.cumtime)
               {
                  active_workout.runtime += elapsed_ms / 1000;
                  var len = active_workout.sections.length;
                  for(var i = 0; i < len; i++)
                  {
                     if ((active_workout.runtime >= active_workout.sections[ i ].cumtime) &&
                         (active_workout.runtime < active_workout.sections[ i ].cumtime + active_workout.sections[ i ].duration))
                     {
                        setValue("speed_demand", active_workout.sections[ i ].speed);
                        break;
                     }
                  }
               }
            }
         }
     });
   });
}


function setValue(name, value)
{
    var success = false;
    var address = 0;

    if (name === "speed_demand_register")
    {
        address = HOLDING_REGISTER_ADDRESS.speed_demand;
        if ((value) && (value >= 0) && (value <= 1023))
        {
            value = Number(value);
            success = true;
        }
    }
    else if (name === "speed_demand_volts")
    {
        address = HOLDING_REGISTER_ADDRESS.speed_demand;
        if ((value) && (value >= 0.0) && (value <= 5.0))
        {
            value *= 1024.0 / 5.0;
            value = Math.round(value);
            value = Math.max(value, 0);
            value = Math.min(value, 1023);
            success = true;
        }
    }
    else if (name === "speed_demand")
    {
        address = HOLDING_REGISTER_ADDRESS.speed_demand;
        if ((value) && (value >= 1) && (value <= 55))
        {
            value--;
            value /= 30;
            value += 1.25;
            value *= 1024.0 / 5.0;
            value = Math.round(value);
            value = Math.max(value, 0);
            value = Math.min(value, 1023);
            success = true;
        }
    }
    else if (name === "speed_actual_filter_COF")
    {
        address = HOLDING_REGISTER_ADDRESS.speed_actual_filter_COF;
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
    else if (name === "supply_power_filter_COF")
    {
        address = HOLDING_REGISTER_ADDRESS.supply_power_filter_COF;
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
    else if (name === "control_active")
    {
        address = HOLDING_REGISTER_ADDRESS.control;
        is_active = value;
        var value = holding_registers[address];
        value &= ~(CONTROL_ACTIVE_BIT);

        if (typeof is_active === "boolean")
        {
            if (is_active)
            {
                value |= CONTROL_ACTIVE_BIT;
            }
            success = true;
        }
    }
    else if (name === "control_deadband_volts")
    {
        address = HOLDING_REGISTER_ADDRESS.control_deadband;
        if ((value) && (value >= 0.0) && (value <= 5.0))
        {
            value *= 1024.0 / 5.0;
            value = Math.round(value);
            value = Math.max(value, 0);
            value = Math.min(value, 1023);
            success = true;
        }
    }
    else if (name === "control_timeconst")
    {
        address = HOLDING_REGISTER_ADDRESS.control_timeconst;
        if ((value) && (value >= 0.0) && (value <= 0xFFFF))
        {
            value = Number(value);
            success = true;
        }
    }

    if (success)
    {
        write_modbus_queue(function ()
        {
            client.request(FC.WRITE_SINGLE_REGISTER, address, value, function (err, MB_response)
            {
                modbus_busy = false;
                if (err) throw err;
                console.log(MB_response);
            });
        });
    }
    return success;
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
           if ((uri === "/system_state/speed_demand_register") ||
               (uri === "/system_state/speed_demand_volts") ||
               (uri === "/system_state/speed_demand") ||
               (uri === "/system_state/speed_actual_filter_COF") ||
               (uri === "/system_state/supply_power_filter_COF") ||
               (uri === "/system_state/control_active") ||
               (uri === "/system_state/control_deadband_volts") ||
               (uri === "/system_state/control_timeconst"))
               {
                  var name = uri.substr(14);
                  value = JSON.parse(body);

                  if (setValue(name, value))
                  {
                     response.writeHead(204);
                     response.end();
                     return;
                  }
                  else
              {
                 response.writeHead(400, { "Content-Type": "text/plain" });
                 response.write("400 Invalid Resourse [" + uri + "]\n");
                 response.end();
                 return;
              }
           }
           else if (uri === "/workouts/active_workout_index")
           {
              value = Number(JSON.parse(body));
              set_active_workout_index(value);
              response.writeHead(204);
              response.end();
              return;
           }
           else if (uri === "/workouts/active_workout/runtime")
           {
              value = Number(JSON.parse(body));
              if (active_workout)
              {
                 if ((value >= 0) && (value <= active_workout.cumtime))
                 {
                    active_workout.runtime = value;
                 }
              }
              response.writeHead(204);
              response.end();
              return;
           }
           else if (uri === "/workouts/active_workout/mode")
           {
              value = JSON.parse(body);
              if (active_workout)
              {
                 if ((value === 'ON') || (value === 'OFF') || (value === 'AUTO'))
                 {
                    active_workout.mode = value;
                 }
              }
              response.writeHead(204);
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
     else if (uri === "/workouts")
     {
        response.writeHead(200, {"Content-Type": "application/json"});
        response.write(JSON.stringify(workouts, function(key, value)
           {
             if (key=="sections")
             {
                return undefined;
             }
             else return value;
          }));
        response.end();
        return;
     }
     else if (uri === "/workouts/active_workout_index")
     {
        response.writeHead(200, {"Content-Type": "application/json"});
        response.write(JSON.stringify(active_workout_index));
        response.end();
        return;
     }
     else if (uri === "/workouts/active_workout")
     {
        response.writeHead(200, {"Content-Type": "application/json"});
        response.write(JSON.stringify(active_workout));
        response.end();
        return;
     }
     else if (uri === "/workouts/active_workout/runtime")
     {
        response.writeHead(200, {"Content-Type": "application/json"});
        response.write(JSON.stringify(active_workout.runtime));
        response.end();
        return;
     }
     else if (uri === "/workouts/active_workout/mode")
     {
        response.writeHead(200, {"Content-Type": "application/json"});
        response.write(JSON.stringify(active_workout.mode));
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
