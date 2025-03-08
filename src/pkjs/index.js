var Clay = require("pebble-clay");
var clayConfig = require("./config.json");
var clay = new Clay(clayConfig);

var CODE_PREFIX_TO_ICON_INDEX = {
  2: 0,
  3: 1,
  5: 1,
  6: 2,
  7: 3,
  8: 4,
};

function weatherIdToIconIndex(weatherId) {
  if (weatherId > 800) {
    return 5;
  }
  return CODE_PREFIX_TO_ICON_INDEX[Math.floor(weatherId / 100)];
}

function getWeatherData() {
  var settings = JSON.parse(localStorage.getItem("clay-settings")) || {};
  var owm_key = settings.OWM_API_KEY;

  if (!owm_key) {
    return;
  }
  var units = settings.UNITS === "F" ? "imperial" : "metric";
  navigator.geolocation.getCurrentPosition(
    function (pos) {
      var xhr = new XMLHttpRequest();
      xhr.onload = function () {
        var response = JSON.parse(xhr.responseText);
        var temperature = Math.round(response.main.temp);
        var conditions = weatherIdToIconIndex(response.weather[0].id);
        Pebble.sendAppMessage(
          {
            UNITS: settings.UNITS || "C",
            TEMPERATURE: temperature,
            CONDITIONS: conditions,
          },
          function (e) {
            console.log("Weather info sent to Pebble successfully!");
          },
          function (e) {
            console.log("Error sending weather info to Pebble!");
          }
        );
      };
      var owm_url =
        "https://api.openweathermap.org/data/2.5/weather?lat=" +
        pos.coords.latitude +
        "&lon=" +
        pos.coords.longitude +
        "&appid=" +
        owm_key +
        "&units=" +
        units;
      xhr.open("GET", owm_url);
      xhr.send();
    },
    function (err) {
      console.log("Error requesting location!");
    },
    {
      timeout: 15000,
      maximumAge: 60000,
    }
  );
}

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
    "https://cloud.appwrite.io/v1" +
      "/databases/6689a86c002a9fb1b740" +
      "/collections/67b3d257002fbfda61e9" +
      "/documents?project=67cc9f770031b2c18340" +
      "&queries[0]=%7B%22method%22:%22orderAsc%22,%22attribute%22:%22net%22%7D" +
      "&queries[1]=%7B%22method%22:%22equal%22,%22attribute%22:%22launched%22,%22values%22:[false]%7D" +
      "&queries[2]=%7B%22method%22:%22limit%22,%22values%22:[1]%7D"
  );
  xhr.send();
}

Pebble.addEventListener("ready", function () {
  getLaunchData();
  getWeatherData();
});

Pebble.addEventListener("appmessage", function (e) {
  if (e.payload.MESSAGE_TYPE === "weather") {
    getWeatherData();
  } else {
    getLaunchData();
  }
});
