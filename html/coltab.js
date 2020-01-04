/*
QO-100 Java Script
by DJ0ABR
Color translation table.
used to assigna a color to a dBm value
*/
var coltab = [
[0.0,0.0,0.0],
[0.6,0.9,1.0],
[0.0,0.0,1.0],
[0.0157,0.0,0.0],
[0.0235,0.0,0.0],
[0.0314,0.0,0.0],
[0.0392,0.0,0.0],
[0.0471,0.0,0.0],
[0.0549,0.0,0.0],
[0.0627,0.0,0.0],
[0.0706,0.0,0.0],
[0.0784,0.0,0.0],
[0.0863,0.0,0.0],
[0.0941,0.0,0.0],
[0.1020,0.0,0.0],
[0.1098,0.0,0.0],
[0.1176,0.0,0.0],
[0.1255,0.0,0.0],
[0.1333,0.0,0.0],
[0.1412,0.0,0.0],
[0.1490,0.0,0.0],
[0.1569,0.0,0.0],
[0.1647,0.0,0.0],
[0.1725,0.0,0.0],
[0.1804,0.0,0.0],
[0.1882,0.0,0.0],
[0.1961,0.0,0.0],
[0.2039,0.0,0.0],
[0.2118,0.0,0.0],
[0.2196,0.0,0.0],
[0.2275,0.0,0.0],
[0.2353,0.0,0.0],
[0.2431,0.0,0.0],
[0.2510,0.0,0.0],
[0.2588,0.0,0.0],
[0.2667,0.0,0.0],
[0.2745,0.0,0.0],
[0.2824,0.0,0.0],
[0.2902,0.0,0.0],
[0.2980,0.0,0.0],
[0.3059,0.0,0.0],
[0.3137,0.0,0.0],
[0.3216,0.0,0.0],
[0.3294,0.0,0.0],
[0.3373,0.0,0.0],
[0.3451,0.0,0.0],
[0.3529,0.0,0.0],
[0.3608,0.0,0.0],
[0.3686,0.0,0.0],
[0.3765,0.0,0.0],
[0.3843,0.0,0.0],
[0.3922,0.0,0.0],
[0.4000,0.0,0.0],
[0.4078,0.0,0.0],
[0.4157,0.0,0.0],
[0.4235,0.0,0.0],
[0.4314,0.0,0.0],
[0.4392,0.0,0.0],
[0.4471,0.0,0.0],
[0.4549,0.0,0.0],
[0.4627,0.0,0.0],
[0.4706,0.0,0.0],
[0.4784,0.0,0.0],
[0.4863,0.0,0.0],
[0.4941,0.0,0.0],
[0.5020,0.0,0.0],
[0.5098,0.0098,0.0],
[0.5176,0.0176,0.0],
[0.5255,0.0255,0.0],
[0.5333,0.0333,0.0],
[0.5412,0.0412,0.0],
[0.5490,0.0490,0.0],
[0.5569,0.0569,0.0],
[0.5647,0.0647,0.0],
[0.5725,0.0725,0.0],
[0.5804,0.0804,0.0],
[0.5882,0.0882,0.0],
[0.5961,0.0961,0.0],
[0.6039,0.1039,0.0],
[0.6118,0.1118,0.0],
[0.6196,0.1196,0.0],
[0.6275,0.1275,0.0],
[0.6353,0.1353,0.0],
[0.6431,0.1431,0.0],
[0.6510,0.1510,0.0],
[0.6588,0.1588,0.0],
[0.6667,0.1667,0.0],
[0.6745,0.1745,0.0],
[0.6824,0.1824,0.0],
[0.6902,0.1902,0.0],
[0.6980,0.1980,0.0],
[0.7059,0.2059,0.0],
[0.7137,0.2137,0.0],
[0.7216,0.2216,0.0],
[0.7294,0.2294,0.0],
[0.7373,0.2373,0.0],
[0.7451,0.2451,0.0],
[0.7529,0.2529,0.0],
[0.7608,0.2608,0.0],
[0.7686,0.2686,0.0],
[0.7765,0.2765,0.0],
[0.7843,0.2843,0.0],
[0.7922,0.2922,0.0],
[0.8000,0.3001,0.0],
[0.8078,0.3078,0.0],
[0.8157,0.3157,0.0],
[0.8235,0.3235,0.0],
[0.8314,0.3314,0.0],
[0.8392,0.3392,0.0],
[0.8471,0.3471,0.0],
[0.8549,0.3549,0.0],
[0.8627,0.3627,0.0],
[0.8706,0.3706,0.0],
[0.8784,0.3784,0.0],
[0.8863,0.3863,0.0],
[0.8941,0.3941,0.0],
[0.9020,0.4020,0.0],
[0.9098,0.4098,0.0],
[0.9176,0.4176,0.0],
[0.9255,0.4255,0.0],
[0.9333,0.4333,0.0],
[0.9412,0.4412,0.0],
[0.9490,0.4490,0.0],
[0.9569,0.4569,0.0],
[0.9647,0.4647,0.0],
[0.9725,0.4725,0.0],
[0.9804,0.4804,0.0],
[0.9882,0.4882,0.0],
[0.9961,0.4961,0.0],
[1.0000,0.5039,0.0],
[1.0000,0.5118,0.0118],
[1.0000,0.5196,0.0196],
[1.0000,0.5275,0.0275],
[1.0000,0.5353,0.0353],
[1.0000,0.5431,0.0431],
[1.0000,0.5510,0.0510],
[1.0000,0.5588,0.0588],
[1.0000,0.5667,0.0667],
[1.0000,0.5745,0.0745],
[1.0000,0.5824,0.0824],
[1.0000,0.5902,0.0902],
[1.0000,0.5980,0.0980],
[1.0000,0.6059,0.1059],
[1.0000,0.6137,0.1137],
[1.0000,0.6216,0.1216],
[1.0000,0.6294,0.1294],
[1.0000,0.6373,0.1373],
[1.0000,0.6451,0.1451],
[1.0000,0.6529,0.1529],
[1.0000,0.6608,0.1608],
[1.0000,0.6686,0.1686],
[1.0000,0.6765,0.1765],
[1.0000,0.6843,0.1843],
[1.0000,0.6922,0.1922],
[1.0000,0.7000,0.2000],
[1.0000,0.7078,0.2078],
[1.0000,0.7157,0.2157],
[1.0000,0.7235,0.2235],
[1.0000,0.7314,0.2314],
[1.0000,0.7392,0.2392],
[1.0000,0.7471,0.2471],
[1.0000,0.7549,0.2549],
[1.0000,0.7627,0.2627],
[1.0000,0.7706,0.2706],
[1.0000,0.7784,0.2784],
[1.0000,0.7863,0.2863],
[1.0000,0.7941,0.2941],
[1.0000,0.8020,0.3020],
[1.0000,0.8098,0.3098],
[1.0000,0.8176,0.3176],
[1.0000,0.8255,0.3255],
[1.0000,0.8333,0.3333],
[1.0000,0.8412,0.3412],
[1.0000,0.8490,0.3490],
[1.0000,0.8569,0.3569],
[1.0000,0.8647,0.3647],
[1.0000,0.8725,0.3725],
[1.0000,0.8804,0.3804],
[1.0000,0.8882,0.3882],
[1.0000,0.8961,0.3961],
[1.0000,0.9039,0.4039],
[1.0000,0.9118,0.4118],
[1.0000,0.9196,0.4196],
[1.0000,0.9275,0.4275],
[1.0000,0.9353,0.4353],
[1.0000,0.9431,0.4431],
[1.0000,0.9510,0.4510],
[1.0000,0.9588,0.4588],
[1.0000,0.9667,0.4667],
[1.0000,0.9745,0.4745],
[1.0000,0.9824,0.4824],
[1.0000,0.9902,0.4902],
[1.0000,0.9980,0.4980],
[1.0000,1.0000,0.5059],
[1.0000,1.0000,0.5137],
[1.0000,1.0000,0.5216],
[1.0000,1.0000,0.5294],
[1.0000,1.0000,0.5373],
[1.0000,1.0000,0.5451],
[1.0000,1.0000,0.5529],
[1.0000,1.0000,0.5608],
[1.0000,1.0000,0.5686],
[1.0000,1.0000,0.5765],
[1.0000,1.0000,0.5843],
[1.0000,1.0000,0.5922],
[1.0000,1.0000,0.6000],
[1.0000,1.0000,0.6078],
[1.0000,1.0000,0.6157],
[1.0000,1.0000,0.6235],
[1.0000,1.0000,0.6314],
[1.0000,1.0000,0.6392],
[1.0000,1.0000,0.6471],
[1.0000,1.0000,0.6549],
[1.0000,1.0000,0.6627],
[1.0000,1.0000,0.6706],
[1.0000,1.0000,0.6784],
[1.0000,1.0000,0.6863],
[1.0000,1.0000,0.6941],
[1.0000,1.0000,0.7020],
[1.0000,1.0000,0.7098],
[1.0000,1.0000,0.7176],
[1.0000,1.0000,0.7255],
[1.0000,1.0000,0.7333],
[1.0000,1.0000,0.7412],
[1.0000,1.0000,0.7490],
[1.0000,1.0000,0.7569],
[1.0000,1.0000,0.7647],
[1.0000,1.0000,0.7725],
[1.0000,1.0000,0.7804],
[1.0000,1.0000,0.7882],
[1.0000,1.0000,0.7961],
[1.0000,1.0000,0.8039],
[1.0000,1.0000,0.8118],
[1.0000,1.0000,0.8196],
[1.0000,1.0000,0.8275],
[1.0000,1.0000,0.8353],
[1.0000,1.0000,0.8431],
[1.0000,1.0000,0.8510],
[1.0000,1.0000,0.8588],
[1.0000,1.0000,0.8667],
[1.0000,1.0000,0.8745],
[1.0000,1.0000,0.8824],
[1.0000,1.0000,0.8902],
[1.0000,1.0000,0.8980],
[1.0000,1.0000,0.9059],
[1.0000,1.0000,0.9137],
[1.0000,1.0000,0.9216],
[1.0000,1.0000,0.9294],
[1.0000,1.0000,0.9373],
[1.0000,1.0000,0.9451],
[1.0000,1.0000,0.9529],
[1.0000,1.0000,0.9608],
[1.0000,1.0000,0.9686],
[1.0000,1.0000,0.9765],
[ 1.0,1.0,1.0],
[ 1.0,1.0,1.0]
];