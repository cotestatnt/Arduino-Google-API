// Finalize Nodejs Script
// 1 - Append JS in HTML Document
// 2 - Gzip HTML
// 3 - Covert to Raw Bytes
// 4 - ( Save to File: webpage.h ) in dist Folder

const fs = require('fs');
const { gzip } = require('@gfx/zopfli');

function getByteArray(file){
    let fileData = file.toString('hex');
    let result = [];
    for (let i = 0; i < fileData.length; i+=2)
      result.push('0x'+fileData[i]+''+fileData[i+1]);
    return result;
}

let js = fs.readFileSync(__dirname+'/dist/js/app.js');
let html = `
<!DOCTYPE html>
<html lang="">
  <head>
    <meta charset="utf-8">
    <meta http-equiv="X-UA-Compatible" content="IE=edge">
    <meta name="viewport" content="width=device-width,initial-scale=1.0">
    <link rel="icon" href="data:image/svg+xml;base64,PHN2ZyB2aWV3Qm94PSIwIDAgMTI4IDEyOCIgeG1sbnM9Imh0dHA6Ly93d3cudzMub3JnLzIwMDAvc3ZnIj4KICA8cmFkaWFsR3JhZGllbnQgaWQ9ImEiIGN4PSItOTYwLjc5IiBjeT0iLTM5NC4zMTkiIHI9IjgxLjkzMyIgZ3JhZGllbnRUcmFuc2Zvcm09Im1hdHJpeCgxLjA1OTY2NywgMCwgMCwgMS4zMDQ4NTQsIDExMTAuNDg0NzQsIDUxMS41OTA0NTQpIiBncmFkaWVudFVuaXRzPSJ1c2VyU3BhY2VPblVzZSI+CiAgICA8c3RvcCBvZmZzZXQ9IjAiIHN0b3AtY29sb3I9IiMwZGE5NjAiLz4KICAgIDxzdG9wIG9mZnNldD0iMSIgc3RvcC1jb2xvcj0iIzAzOTE0YiIvPgogIDwvcmFkaWFsR3JhZGllbnQ+CiAgPHJhZGlhbEdyYWRpZW50IGlkPSJiIiBjeD0iLTk1OC4wNDIiIGN5PSItMzQyLjk2NSIgcj0iNjAuNzczIiBncmFkaWVudFRyYW5zZm9ybT0ibWF0cml4KDEuMDU5NjY3LCAwLCAwLCAxLjMwNDg1NCwgMTExMC40ODQ3NCwgNTExLjU5MDQ1NCkiIGdyYWRpZW50VW5pdHM9InVzZXJTcGFjZU9uVXNlIj4KICAgIDxzdG9wIG9mZnNldD0iMCIgc3RvcC1jb2xvcj0iI2ZmY2Q0ZCIvPgogICAgPHN0b3Agb2Zmc2V0PSIxIiBzdG9wLWNvbG9yPSIjZjZjMzM4Ii8+CiAgPC9yYWRpYWxHcmFkaWVudD4KICA8bGluZWFyR3JhZGllbnQgaWQ9ImMiIGdyYWRpZW50VW5pdHM9InVzZXJTcGFjZU9uVXNlIiB4MT0iLTkzOC44MzQiIHkxPSItMzMxLjE3IiB4Mj0iLTk2NC43MDUiIHkyPSItMzMxLjE3IiBncmFkaWVudFRyYW5zZm9ybT0ibWF0cml4KDEuMDU5NjY3LCAwLCAwLCAxLjMwNDg1NCwgMTExMC40ODQ3NCwgNTExLjU5MDQ1NCkiPgogICAgPHN0b3Agb2Zmc2V0PSIwIiBzdG9wLWNvbG9yPSIjMjMxZjIwIiBzdG9wLW9wYWNpdHk9Ii4yIi8+CiAgICA8c3RvcCBvZmZzZXQ9IjEiIHN0b3AtY29sb3I9IiNmMWYyZjIiIHN0b3Atb3BhY2l0eT0iLjI1Ii8+CiAgPC9saW5lYXJHcmFkaWVudD4KICA8cGF0aCBmaWxsPSJ1cmwoI2EpIiBkPSJNIDEwOS43MTkgOTAuNTA5IEMgMTEwLjk4NSA5My4wODUgMTExLjAwOSA5Ny4xNDYgMTA5LjkyIDk5Ljk0IEwgMTI0Ljk5MSA2OS4wMzIgQyAxMjYuMzM2IDY2LjI2MyAxMjYuMzM2IDYxLjc0OCAxMjQuOTkxIDU4Ljk5MSBMIDEwMC42MzQgOS4wMiBDIDk5LjI4MSA2LjI2OCA5Ni4wMzkgNCA5My40MjEgNCBMIDcyLjE5MiA0IEMgNjkuNTgxIDQgNjguNTQgNi4yNjcgNjkuODg3IDkuMDIyIEwgODUuMDUyIDQwLjEwOCBDIDg1LjE0NCA0MC4yNjYgODUuMjc0IDQwLjQwNiA4NS4zNTUgNDAuNTUyIEwgMTA5LjcxOSA5MC41MDkgWiIvPgogIDxwYXRoIGZpbGw9InVybCgjYikiIGQ9Ik0gNzIuMTkyIDEyMy45OTkgTCA5My40MjEgMTIzLjk5OSBDIDk2LjA0IDEyMy45OTkgOTkuMjggMTIxLjc1IDEwMC42MzUgMTE4Ljk3OCBMIDEwOS45MjUgOTkuOTM3IEMgMTExLjAxIDk3LjE0MiAxMTAuOTg4IDkzLjA4MyAxMDkuNzIzIDkwLjUwNiBMIDk0LjI2MiA1OC45ODMgQyA5NS42MDYgNjEuNzM5IDk1LjYwNiA2Ni4yNTUgOTQuMjYyIDY5LjAyNCBMIDc5LjE4MiA5OS45MTUgQyA3OS4xMDcgMTAwLjExOSA3OS4wNjcgMTAwLjM2NCA3OC45ODYgMTAwLjU1MyBMIDY5LjczIDExOS41MjQgQyA2OC43ODEgMTIyLjAyMyA2OS43NTcgMTIzLjk5OSA3Mi4xOTIgMTIzLjk5OSBaIi8+CiAgPHBhdGggZmlsbD0idXJsKCNjKSIgZD0iTSAxMDkuNzE5IDkwLjUwOSBMIDk0LjI1OSA1OC45ODggQyA5NS42MDMgNjEuNzQ0IDk1LjYwMyA2Ni4yNTkgOTQuMjU5IDY5LjAyOCBMIDg4LjgxMSA4MC4xOTggTCAxMDkuOTE4IDk5Ljk0IEMgMTExLjAwOSA5Ny4xNDYgMTEwLjk4NSA5My4wODQgMTA5LjcxOSA5MC41MDkgWiIvPgogIDxnIHRyYW5zZm9ybT0ibWF0cml4KDEuMDU5NjY2LCAwLCAwLCAxLjMwNDg1NCwgMi4wMDEzMjUsIDQuMDAwNDUxKSI+CiAgICA8cmFkaWFsR3JhZGllbnQgaWQ9ImQiIGN4PSItMTM1NC4zOTIiIGN5PSItMTE5NC4xNDYiIHI9IjgxLjkzNSIgZ3JhZGllbnRUcmFuc2Zvcm09InJvdGF0ZSgxODAgLTY2MS4zMjUgLTU0OC40MzIpIiBncmFkaWVudFVuaXRzPSJ1c2VyU3BhY2VPblVzZSI+CiAgICAgIDxzdG9wIG9mZnNldD0iMCIgc3RvcC1jb2xvcj0iIzQzODdmZCIvPgogICAgICA8c3RvcCBvZmZzZXQ9IjEiIHN0b3AtY29sb3I9IiM0NjgzZWEiLz4KICAgIDwvcmFkaWFsR3JhZGllbnQ+CiAgICA8cGF0aCBmaWxsPSJ1cmwoI2QpIiBkPSJNMTUuMzY1IDI1LjY2NWMtMS4xODEtMS45NzYtMS4yMi01LjA4OC0uMTgyLTcuMjI4TC45NTIgNDIuMTNjLTEuMjcxIDIuMTIyLTEuMjcxIDUuNTgzIDAgNy42OThsMjMgMzguMjg2YzEuMjY5IDIuMTEzIDQuMzMyIDMuODUgNi43OTEgMy44NWgyMC4wMzVjMi40NzMgMCAzLjQ1NS0xLjczNSAyLjE4NC0zLjg1TDM4LjY0NSA2NC4yOTJjLS4wOC0uMTIzLS4yMS0uMjI5LS4yODEtLjM0MkwxNS4zNjUgMjUuNjY1eiIvPgogICAgPHJhZGlhbEdyYWRpZW50IGlkPSJlIiBjeD0iLTEzNTEuNjQ0IiBjeT0iLTExNDIuNzk1IiByPSI2MC43NzIiIGdyYWRpZW50VHJhbnNmb3JtPSJtYXRyaXgoLTAuOTk5NzgyLCAwLCAwLCAtMSwgLTEzMjIuMzU5MjQ1LCAtMTA5Ni44NjQwMTQpIiBncmFkaWVudFVuaXRzPSJ1c2VyU3BhY2VPblVzZSI+CiAgICAgIDxzdG9wIG9mZnNldD0iMCIgc3RvcC1jb2xvcj0iI2UwNGEzZiIvPgogICAgICA8c3RvcCBvZmZzZXQ9IjEiIHN0b3AtY29sb3I9IiNjZDM3MmQiLz4KICAgIDwvcmFkaWFsR3JhZGllbnQ+CiAgICA8cGF0aCBmaWxsPSJ1cmwoI2UpIiBkPSJNIDUwLjc3MSAwIEwgMzAuNzQgMCBDIDI4LjI4MiAwIDI1LjIyIDEuNzI3IDIzLjk1MSAzLjg0OSBMIDE1LjE4MyAxOC40MzkgQyAxNC4xNDYgMjAuNTc5IDE0LjE4NSAyMy42OTEgMTUuMzY1IDI1LjY2NyBMIDI5Ljk2IDQ5LjgzIEMgMjguNjkgNDcuNzE1IDI4LjY5IDQ0LjI1MyAyOS45NiA0Mi4xMzIgTCA0NC4xODEgMTguNDU4IEMgNDQuMjUgMTguMyA0NC4yODUgMTguMTE4IDQ0LjM3OSAxNy45NjYgTCA1My4xMDYgMy40MyBDIDUzLjk5NyAxLjUxOCA1My4wNjkgMCA1MC43NzEgMCBaIi8+CiAgICA8bGluZWFyR3JhZGllbnQgaWQ9ImYiIGdyYWRpZW50VW5pdHM9InVzZXJTcGFjZU9uVXNlIiB4MT0iLTEzMzIuNDM0IiB5MT0iLTExMzAuOTk3IiB4Mj0iLTEzNTguMzEiIHkyPSItMTEzMC45OTciIGdyYWRpZW50VHJhbnNmb3JtPSJtYXRyaXgoLTAuOTk5NzgyLCAwLCAwLCAtMSwgLTEzMjIuMzU5MjQ1LCAtMTA5Ni44NjQwMTQpIj4KICAgICAgPHN0b3Agb2Zmc2V0PSIwIiBzdG9wLWNvbG9yPSIjMjMxZjIwIiBzdG9wLW9wYWNpdHk9Ii4yIi8+CiAgICAgIDxzdG9wIG9mZnNldD0iMSIgc3RvcC1jb2xvcj0iI2YxZjJmMiIgc3RvcC1vcGFjaXR5PSIuMjUiLz4KICAgIDwvbGluZWFyR3JhZGllbnQ+CiAgICA8cGF0aCBmaWxsPSJ1cmwoI2YpIiBkPSJNIDE1LjM2NSAyNS42NjUgTCAyOS45NiA0OS44MjggQyAyOC42OSA0Ny43MTMgMjguNjkgNDQuMjUxIDI5Ljk2IDQyLjEzIEwgMzUuMDk5IDMzLjU3MiBMIDE1LjE4NCAxOC40MzggQyAxNC4xNDcgMjAuNTc3IDE0LjE4NiAyMy42OSAxNS4zNjUgMjUuNjY1IFoiLz4KICA8L2c+Cjwvc3ZnPg==">
    <title>Arduino Google API</title>
  </head>
  <body>
    <noscript>
      <strong>Sorry, but Arduino Google API doesn't work without JavaScript. Please enable to continue.</strong>
    </noscript>
    <div id="app"></div>
    <script>${js}</script>
  </body>
</html>
`;
// Gzip the index.html file with JS Code.
// const gzippedIndex = zlib.gzipSync(html, {'level': zlib.constants.Z_BEST_COMPRESSION});
// let indexHTML = getByteArray(gzippedIndex);

// let source =
// `
// const uint32_t WEBPAGE_HTML_SIZE = ${indexHTML.length};
// const char WEBPAGE_HTML[] PROGMEM = { ${indexHTML} };
// `;
// fs.writeFileSync(__dirname+'/dist/webpage.h', source, 'utf8');

// Produce a second variant with zopfli (a improved zip algorithm by google)
// Takes much more time and maybe is not available on every machine
const input =  html;
gzip(input, {numiterations: 15}, (err, output) => {
    indexHTML = output;
    let source =
`
const uint32_t WEBPAGE_HTML_SIZE = ${indexHTML.length};
const char WEBPAGE_HTML[] PROGMEM = { ${indexHTML} };
`;

fs.writeFileSync(__dirname + '/dist/webpage.h', source, 'utf8');
//  fs.writeFileSync('../Arduino-Google-API/src/WebServer/setup_htm.h', source, 'utf8');

});
