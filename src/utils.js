function sleep(ms) {
   const start = new Date().getTime() + ms;

   while (start > new Date().getTime()) {}
}

function isPrime(num, delay) {
   if (delay === true) sleep(num);

   if (num % 2 == 0) {
      return num == 2 ? true : false;
   }

   for (let i = 3; i * i <= num; i += 2) {
      if (num % i == 0) return false;
   }

   return true;
}

function buf2num(buf) {
   let num = 0;
   const code0 = '0'.charCodeAt(0);
   const code9 = '9'.charCodeAt(0);

   for (let i = 0; i < buf.length; i++) {
      if (buf[i] >= code0 && buf[i] <= code9) {
         num = num * 10 + buf[i] - code0;
      } else {
         break;
      }
   }

   return num;
}

module.exports = {
   buf2num,
   isPrime,
};
