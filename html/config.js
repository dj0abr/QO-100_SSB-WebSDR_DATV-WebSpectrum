var config = {
    call:               'MYCALL',
    websock_port:       1234,
    allowRemoteAccess:  5,
    lnb_orig:           67,
    lnb_crystal:        89,
    downmixer_outqrg:   10,
    minitiouner_ip:     '192.168.0.25',
    minitiouner_port:   1112,
    minitiouner_local:  13,
    CIV_address:        0xa2,
    tx_correction:      0,
    icom_satmode:       0,
    pluto_ip:           0,
}

var configname = 'config2'

function get32(myarr)
{
    var val = myarr[0];
    val <<= 8;
    val += myarr[1];
    val <<= 8;
    val += myarr[2];
    val <<= 8;
    val += myarr[3];
    return val;
}

function extract_data(arr, idx)
{
    mycall = String.fromCharCode.apply(String, arr.slice(idx+0,idx+20));
    for(i=0; i<20; i++) mycall = mycall.replace('\0',' ');
    config.call = mycall.trim().toUpperCase();
    //console.log("Callsign:<" + config.call + ">");
    
    config.websock_port = get32(arr.slice(idx+20));
    //console.log("websock_port:" + config.websock_port);
    
    config.allowRemoteAccess = get32(arr.slice(idx+24));
    //console.log("allowRemoteAccess:" + config.allowRemoteAccess);
    
    config.lnb_orig = get32(arr.slice(idx+28));
    //console.log("lnb_orig:" + config.lnb_orig);
    
    config.lnb_crystal = get32(arr.slice(idx+32));
    //console.log("lnb_crystal:" + config.lnb_crystal);

    config.downmixer_outqrg = get32(arr.slice(idx+36));
    //console.log("downmixer_outqrg:" + config.downmixer_outqrg);
    
    mip = String.fromCharCode.apply(String, arr.slice(idx+40,idx+40+20));
    for(i=0; i<20; i++) mip = mip.replace('\0',' ');
    config.minitiouner_ip = mip.trim();
    //console.log("minitiouner_ip:<" + config.minitiouner_ip + ">");

    config.minitiouner_port = get32(arr.slice(idx+60));
    //console.log("minitiouner_port:" + config.minitiouner_port);

    config.minitiouner_local = get32(arr.slice(idx+64));
    //console.log("minitiouner_local:" + config.minitiouner_local);
    
    config.CIV_address = get32(arr.slice(idx+68));
    //console.log("CIV_address:" + config.CIV_address + " Hex: " + config.CIV_address.toString(16));
    
    config.tx_correction = get32(arr.slice(idx+72));
    //console.log("tx_correction:" + config.tx_correction);
    
    config.icom_satmode = get32(arr.slice(idx+76));
    //console.log("icom_satmode:" + config.icom_satmode);

    plip = String.fromCharCode.apply(String, arr.slice(idx+80,idx+80+20));
    for(i=0; i<20; i++) plip = plip.replace('\0',' ');
    config.pluto_ip = plip.trim();
    //console.log("pluto_ip:<" + config.pluto_ip + ">");
    
    // next index at 100
    
    localStorage.setItem(configname,JSON.stringify(config));
}

function getConfig()
{
    config = JSON.parse(localStorage.getItem(configname));
    /*
    console.log("Read Config:");
    console.log("Callsign:<" + config.call + ">");
    console.log("websock_port:" + config.websock_port);
    console.log("allowRemoteAccess:" + config.allowRemoteAccess);
    console.log("lnb_orig:" + config.lnb_orig);
    console.log("lnb_crystal:" + config.lnb_crystal);
    console.log("downmixer_outqrg:" + config.downmixer_outqrg);
    console.log("minitiouner_ip:<" + config.minitiouner_ip + ">");
    console.log("minitiouner_port:" + config.minitiouner_port);
    console.log("minitiouner_local:" + config.minitiouner_local);
    console.log("CIV_address:" + config.CIV_address);
    console.log("tx_correction:" + config.tx_correction);
    console.log("icom_satmode:" + config.icom_satmode);
    console.log("pluto_ip:<" + config.pluto_ip + ">");
    */
}

function saveConfig()
{
    localStorage.setItem(configname,JSON.stringify(config));
}

// send setupdata via websocket
function sendSetup()
{
    getConfig();
    
    var txstr = "cfgdata:" +
    config.call + ";" +
    config.websock_port + ";" +
    config.allowRemoteAccess + ";" +
    config.lnb_orig + ";" +
    config.lnb_crystal + ";" +
    config.downmixer_outqrg + ";" +
    config.minitiouner_ip + ";" +
    config.minitiouner_port + ";" +
    config.minitiouner_local + ";" +
    config.CIV_address + ";" +
    config.tx_correction + ";" + 
    config.icom_satmode + ";" +
    config.pluto_ip + ";"
    ; 

    //console.log(txstr);
        
    websocket.send(txstr);
}
