const { isPrime } = require('./utils');

process.on('message', message => {
   console.log('[child %d] received msg from server:', process.pid, message);

   process.send({ task: message, result: isPrime(message, true) });
   process.disconnect();

   console.log('[child %d] exiting', process.pid);
   process.exit();
});
