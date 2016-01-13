var FC = require('./modbus-stack').FUNCTION_CODES;
var client = require('./modbus-stack/client').createClient(502);

var http = require("http"),
    url = require("url"),
    path = require("path"),
    fs = require("fs"),
    port = process.argv[2] || 8888;




var speed_demand = 1;

var holding_registers = [];

setTimeout(read_registers, 1000);

function read_registers()
{
   client.request(FC.READ_HOLDING_REGISTERS, 0, 10, function(err, response) {
      if (err) throw err;
      console.log(response);
      for(k = 0; k < 10; k++)
      {
         holding_registers[ k ] = response[ k ];
      }
      setTimeout(read_registers, 1000);

      /*client.request(FC.WRITE_SINGLE_REGISTER, 2, speed_demand, function(err, response) {
              if (err) throw err;
              console.log(response);
              //client.end();
              setTimeout(read_registers, 1000);
              });*/
     });
}



http.createServer(function(request, response) {

  var uri = url.parse(request.url).pathname
    , filename = path.join(process.cwd(), uri);

  console.log(uri);
  console.log(request.method);

  if (request.method === "PUT")
  {
     if (uri === "/holding_registers")
     {
        var url_parts = url.parse(request.url, true);
        var query = url_parts.query;
        console.log(query);

        client.request(FC.WRITE_SINGLE_REGISTER, Number(query.addr), Number(query.value), function(err, MB_response) {
              if (err) throw err;
              console.log(MB_response);
              });

        response.writeHead(200);
        response.end();
        return;
     }
  }

  if (uri === "/holding_registers.json")
  {
     response.writeHead(200);
     response.write(JSON.stringify(holding_registers));
     response.end();
     return;
  }


  fs.exists(filename, function(exists) {
    if(!exists) {
      response.writeHead(404, {"Content-Type": "text/plain"});
      response.write("404 Not Found\n");
      response.end();
      return;
    }

    if (fs.statSync(filename).isDirectory()) filename += '/index.html';

    fs.readFile(filename, "binary", function(err, file) {
      if(err) {        
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
}).listen(parseInt(port, 10));

console.log("Static file server running at\n  => http://localhost:" + port + "/\nCTRL + C to shutdown");

