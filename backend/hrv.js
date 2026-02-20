function computeTimeDomain(rr) {
    if (rr.length < 2) return {};

    const diffs = rr.slice(1).map((v,i)=>v-rr[i]);
    const rmssd = Math.sqrt(diffs.map(d=>d*d).reduce((a,b)=>a+b)/diffs.length);

    const mean = rr.reduce((a,b)=>a+b)/rr.length;
    const sdnn = Math.sqrt(rr.map(v=>(v-mean)**2).reduce((a,b)=>a+b)/rr.length);

    const nn50 = diffs.filter(d=>Math.abs(d)>50).length;
    const pnn50 = (nn50/rr.length)*100;

    return { rmssd, sdnn, pnn50 };
}

module.exports = { computeTimeDomain };
