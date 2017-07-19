// note that the compiled addon is placed under following path
const { OSKShow, OSKClose, OSKSetPosition } = require('./build/Debug/addon');

console.log(OSKClose());
console.log(OSKSetPosition(0,0,800,300));
console.log(OSKShow());

const readline = require('readline');
const rl = readline.createInterface({
    input: process.stdin,
    output: process.stdout,
    prompt: 'OHAI> '
});

rl.prompt();
