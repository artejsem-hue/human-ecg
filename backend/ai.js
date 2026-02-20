function computeRisk(data){

    let risk = 0;

    if(data.bpm > 100) risk += 15;
    if(data.rmssd < 25) risk += 20;
    if(data.sdnn > 90) risk += 20;

    return Math.min(risk,100);
}

module.exports = { computeRisk };
