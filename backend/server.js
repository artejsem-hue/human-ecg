const WebSocket = require('ws');
const { computeTimeDomain } = require('./hrv');
const { computeRisk } = require('./ai');

const wss = new WebSocket.Server({ port: 8080 });

let rrBuffer = [];

wss.on('connection', ws => {

    ws.on('message', message => {

        let data;

        try{
            data = JSON.parse(message);
        } catch {
            return;
        }

        if(data.rr){
            rrBuffer.push(data.rr);
            if(rrBuffer.length > 300)
                rrBuffer.shift();
        }

        const hrv = computeTimeDomain(rrBuffer);
        const risk = computeRisk({...data, ...hrv});

        const enriched = {
            ...data,
            ...hrv,
            risk
        };

        wss.clients.forEach(client=>{
            if(client.readyState === WebSocket.OPEN){
                client.send(JSON.stringify(enriched));
            }
        });
    });

});

console.log("HUMAN ECG backend running on port 8080");
