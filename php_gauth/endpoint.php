<!DOCTYPE html>
<html>
<head>
	<meta charset="utf-8" />
	<meta name="viewport" content="initial-scale = 1.0" />
	<title>OAuth2.0 endpoint</title>
	<link rel="stylesheet" href="https://stackpath.bootstrapcdn.com/bootstrap/4.3.1/css/bootstrap.min.css" integrity="sha384-ggOyR0iXCbMQv3Xipma34MD+dH/1fQ784/j6cY/iJTQUOhcWr7x9JvoRxT2MZw1T" crossorigin="anonymous">
</head>

<body>

<?php
$auth_code =  $_GET['code'];
$pathToFile = 'auth_code.txt';

file_put_contents($pathToFile, "");                         // Clear the file
file_put_contents($pathToFile, $auth_code, FILE_APPEND);    // Add the last authorization coe
?>

<div class="container-fluid" style="padding: 40px;">
    <div class="alert alert-success" role="alert">
      <img src="smart-air.png">
      <br><br>
      <h4 class="alert-heading">Well done!</h4>
      <p>Authorization code was succesfully generated</p>
      <hr>
      <br><br>
      <p class="mb-0">Please, close this window and click on button <br><b>Exchange authorization code for tokens</b></p>
    </div>
</div>


</body>
