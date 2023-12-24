<?php
// Date in the past
header("Cache-Control: no-cache");
header("Pragma: no-cache");
header("Access-Control-Allow-Headers: *");
header("Access-Control-Allow-Origin: *");
header('Access-Control-Allow-Credentials: true');
?>

<?php
// If strict types are enabled i.e. declare(strict_types=1);
//$file = file_get_contents('./auth_code.txt', true);
// Otherwise
$file = file_get_contents('./auth_code.txt', FILE_USE_INCLUDE_PATH);
echo $file;
?>

