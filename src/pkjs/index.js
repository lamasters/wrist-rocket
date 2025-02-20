function getLaunchData() {
  var xhr = new XMLHttpRequest();
  xhr.onload = function () {
    var response = JSON.parse(xhr.responseText);
    if (response.documents) {
      var timeToLaunch = new Date(response.documents[0].net) - new Date();
      var minutesToLaunch = Math.round(timeToLaunch / 1000 / 60);

      var rocketName = response.documents[0].rocket;
      Pebble.sendAppMessage(
        {
          MINUTES_TO_LAUNCH: minutesToLaunch,
          ROCKET: rocketName,
        },
        function (e) {
          console.log("Launch data sent to Pebble successfully!");
        },
        function (e) {
          console.log("Error sending launch data to Pebble!");
        }
      );
    }
  };
  xhr.open(
    "GET",
    "http://homelab.hippogriff-lime.ts.net/v1" +
      "/databases/6689a86c002a9fb1b740" +
      "/collections/67b3d257002fbfda61e9" +
      "/documents?project=65aad3806c956cf09df4" +
      "&queries[0]=%7B%22method%22:%22orderAsc%22,%22attribute%22:%22net%22%7D" +
      "&queries[1]=%7B%22method%22:%22equal%22,%22attribute%22:%22launched%22,%22values%22:[false]%7D" +
      "&queries[2]=%7B%22method%22:%22limit%22,%22values%22:[1]%7D"
  );
  xhr.send();
}

Pebble.addEventListener("ready", function () {
  getLaunchData();
});

Pebble.addEventListener("appmessage", function (e) {
  getLaunchData();
});
