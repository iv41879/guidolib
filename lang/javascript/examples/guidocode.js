var guidoEngine;
function processGMNDiv( div, parser) {
	var content = div.innerHTML;
	content = content.replace(/&lt;/g, "<").replace(/&gt;/g, ">");
	var ar = guidoEngine.string2AR(parser, content);
	var gr = guidoEngine.ar2gr(ar);
	div.innerHTML = guidoEngine.gr2SVG(gr, 1, true, 0);
	guidoEngine.freeGR(gr);
	guidoEngine.freeAR(ar);		
}

function processGMNCode() {
	var divs = document.querySelectorAll(".guido-code");
	var p = guidoEngine.openParser();
	divs.forEach( function(div) { processGMNDiv (div, p) });
	guidoEngine.closeParser(p);
}

document.onreadystatechange = function start() {  
	if (document.readyState == "complete") {
		var module = GuidoModule();
		guidoEngine = new module.GuidoEngineAdapter;
		guidoEngine.init();
		processGMNCode();
	}
} 
