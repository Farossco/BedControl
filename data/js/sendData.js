var sendDataTimeout;

var data;
var resources;

/******************************** Web queries ********************************/

function getResources() {
	var xhttp = new XMLHttpRequest();

	xhttp.open("GET", "getRes");
	xhttp.send();

	xhttp.onreadystatechange = function () {
		if (this.readyState == 4 && this.status == 200) {
			resources = JSON.parse(this.responseText);
			sendData();
		}
	};
}

function sendData(type = 'RQD', value = 0, comp = 0) {
	/* Used to test on codepen.io
	if (type == "LON") data.Light.On = value;
	if (type == "LMO") data.Light.Mode = value;
	if (type == "RGB") data.Light.Rgb[comp] = parseInt(value, 16);
	*/

	var xhttp = new XMLHttpRequest();

	xhttp.open("GET", "command?type=" + type + "&value=" + value + "&comp=" + comp, true);
	xhttp.send();

	xhttp.onreadystatechange = function () {
		if (this.readyState == 4 && this.status == 200) {
			data = JSON.parse(this.responseText);

			displayPage();

			clearTimeout(sendDataTimeout);
			sendDataTimeout = setTimeout(sendData, 2000);
		}
	};
}

/******************************** Display ********************************/

function displayPage() {
	displayLumos();
	displayLightMode();
	displayActions();
}

function displayLumos() {
	document.getElementById("lumos").innerHTML =
		`
			<button class="lumos${data.Light["On"] ? " button-primary" : ""}" onclick = "sendData('LON', ${data.Light["On"] ? 0 : 1})">
				Lumos
			</button>
		`;
}

function displayLightMode() {
	document.getElementById("lightModes").innerHTML =
		`
			<div class="row">
				<h2>Light modes</h2>
			</div>
			${resources.Light.ModeNames.map(mapModenames).join("")}
		`;
}

// Generate HTML for the action section
function displayActions() {
	document.getElementById("actions").innerHTML = ""; // Clear the actions section

	if (data.Light.Mode == 0 || data.Light.Mode == 2 || data.Light.Mode == 3 || data.Light.Mode == 5 || data.Light.Mode == 6) {
		displayColors();
	}
	else {
	}
}

function displayColors() {
	document.getElementById("actions").innerHTML +=
		`
			<div class="row">
				<h2>Color</h2>
			</div>
			${resources.Light.Colors.map(mapColors).join("")}
		`;
}

/******************************** Maping ********************************/

function mapModenames(name, index) {
	return `
		<div class="row">
			<button
				class="lightMode${index == data.Light.Mode ? " button-primary" : ""}"
				onclick = "sendData('LMO', ${index})"
			>${name}</button>
		</div>
	`;
}

var numberList = ["zero", "one", "two", "three", "four", "five", "six"]

var mapColorRow = function (rowSize) {
	return function (displayColor, index) {
		var currentMode = data.Light.Mode;
		var currentColor = data.Light.Rgb[currentMode];
		var displayColorStr = str2hex(displayColor);

		return `
				<button
						class="color ${numberList[rowSize]}${displayColor == currentColor ? " selected" : ""}"
						style="background-color: #${displayColorStr};"
						onclick = "sendData('RGB', '${displayColor}', ${currentMode})"
				></button>
			`;
	};
};

function mapColors(colorRow, index) {
	actualRow = []
	previousItem = -1

	for (item of colorRow) {
		if (item != previousItem) {
			actualRow.push(item)
			previousItem = item
		}
	}

	return `
			<div class="row">
				${actualRow.map(mapColorRow(actualRow.length)).join("")}
			</div>
		`;

}

/******************************** Utils ********************************/

function str2hex(value) {
	str = value.toString(16);

	while (str.length < 6)
		str = "0" + str;

	return str
}

/******************************** Main ********************************/

getResources();