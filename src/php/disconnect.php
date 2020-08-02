<!--  PHP source code for controlling the RaSCSI - 68kmla edition with a web interface. -->
<!--  Copyright (c) 2020 akuker -->
<!--  Distributed under the BSD-3 Clause License -->
<!DOCTYPE html>
<html>
    <head>
  <link rel="stylesheet" href="rascsi_styles.css">
</head>

<body>
<?php
	include 'lib_rascsi.php';
	html_generate_header();

        if(isset($_GET['id'])){
	   // If we're passed an ID, then we should disconnect that
           // device.
	   $cmd = "rasctl -i ".$_GET['id']." -c detach";
	   $result = exec($cmd);
	   echo '<br>';
	   echo 'Ran command: <pre>'.$cmd.'</pre>';
	   echo '<br>';

	   // Check to see if the command succeeded
           if(strlen($result) > 0){
		html_generate_warning($result);
	   }
	   else {
		html_generate_success_message();
	   }
	}
	else {
		html_generate_warning('Page opened without arguments');
	}
	echo '<br>';
	html_generate_ok_to_go_home();
?>
</body>
</html>
