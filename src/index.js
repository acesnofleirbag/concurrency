const net = require('net');
const { buf2num, isPrime } = require('./utils');
const { fork } = require('child_process');

const port = 8081;
const server = net.createServer();


function handleConnection(conn) {
   const remoteAddr = `${conn.remoteAddress}:${conn.remotePort}`;

   console.log('peer %s connected', remoteAddr);

   async function onData(data) {
      const num = buf2num(data);

      console.log('num %d', num);

      const response = await new Promise((resolve, reject) => {
         const worker = fork('./worker.js');

         worker.send(num);
         worker.on('message', message => {
            const response = message.result ? 'prime' : 'composite';

            resolve(response);
         });

         worker.on('error', message => { reject(message) });
      });

      // const response = isPrime(num) ? 'prime' : 'composite';

      conn.write(response + '\n');

      // console.log('... %d is %s', num, response);
   }

   function onClose () {
      console.log('connection %s closed', remoteAddr);
   }

   function onError(err) {
      console.log('connection %s error: %s', remoteAddr, err.message);
   }

   conn.on('data', onData);
   conn.once('close', onClose);
   conn.on('error', onError);
}

server.on('connection', handleConnection);

server.listen(port, () => console.log('listening on port: %d', port))
