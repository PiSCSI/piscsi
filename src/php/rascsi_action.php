<!--  PHP source code for controlling the RaSCSI - 68kmla edition with a web interface. -->
<!--  Copyright (c) 2020 akuker -->
<!--  Distributed under the BSD-3 Clause License -->
<!DOCTYPE html>
<html>

<head>
   <title>RaSCSI Action Page</title>
   <link rel="stylesheet" href="rascsi_styles.css">
</head>

<body>
    <?php
	include 'lib_rascsi.php';
	html_generate_header();

   echo '<br>';
   if($GLOBALS['DEBUG_ENABLE']){
      echo '<table>'.PHP_EOL;
      echo '  <tr><td><p style="color:gray">Debug stuff</p></td></tr>'.PHP_EOL;
      echo '  <tr><td>';

      echo '<p style="color:gray">Post values......................'.PHP_EOL;
      echo '<br> '.PHP_EOL;
      var_dump($_POST);
      echo '<br><br>Running command.... '.$_POST['command'].PHP_EOL;
      echo '<br></p>'.PHP_EOL;
      echo '</td></tr></table>';
   }

	if(isset($_POST['command']))
	{
		switch(strtolower($_POST['command'])){
			case "eject_disk":
				action_eject_disk();
				break;
			case "remove_device":
				action_remove_device();
				break;
			case "connect_new_device":
				action_connect_new_device();
				break;
			case "insert_disk":
				action_insert_disk();
				break;
			case "delete_file":
				action_delete_file();
				break;
			case "create_new_image":
				action_create_new_image();
				break;
			case "restart_rascsi_service":
				action_restart_rascsi_service();
				break;
			case "stop_rascsi_service":
				action_stop_rascsi_service();
				break;
			case "reboot_raspberry_pi":
				action_reboot_raspberry_pi();
				break;
			case "shutdown_raspberry_pi":
				action_shutdown_raspberry_pi();
				break;
			default:
				action_unknown_command();
				break;
      }
	}
   else{
      html_generate_warning("HTTP command was missing POST information. Are you trying to access this page directly? That won't work");
      echo "<br>".PHP_EOL;
      html_generate_ok_to_go_home();
   }

function action_eject_disk(){
   $command = 'rasctl -i '.$_POST['id'].' -c eject 2>&1'.PHP_EOL;
   exec($command, $retArray, $result);
   check_result($result, $command,$retArray);
   html_generate_ok_to_go_home();
}

function action_remove_device(){
	// Check to see if the user has confirmed
	if(isset($_POST['confirmed'])){
		$command = 'rasctl -i '.$_POST['id'].' -c disconnect 2>&1'.PHP_EOL;
		exec($command, $retArray, $result);
		check_result($result, $command,$retArray);
		html_generate_ok_to_go_home();
	}
	else{
		check_are_you_sure('Are you sure you want to disconnect SCSI ID ' . $_POST['id'].'? If the host is running, this could cause undesirable behavior.');
	}
}
// function action_connect_new_device(){}
function action_insert_disk(){
   $command = 'rasctl -i '.$_POST['id'].' -c insert -f '.$GLOBALS['FILE_PATH'].'/'.$_POST['file_name'].' 2>&1'.PHP_EOL;
   exec($command, $retArray, $result);
   check_result($result, $command,$retArray);
   html_generate_ok_to_go_home();
}
function action_create_new_image(){
	// If we already know the size & filename, we can go create the image...
	if(isset($_POST['size']) && isset($_POST['file_name'])){
		$command = 'dd if=/dev/zero of='.$GLOBALS['FILE_PATH'].'/'.$_POST['file_name'].' bs=1M count='.$_POST['size'];
		exec($command, $retArray, $result);
		check_result($result, $command, $retArray);
		html_generate_ok_to_go_home();
	}
	else{
		echo '<h2>Create a new empty file</h2>'.PHP_EOL;
		echo '<form action=rascsi_action.php method="post">'.PHP_EOL;
		echo '   <input type="hidden" name="command" value="'.$_POST['command'].'"/>'.PHP_EOL;
		echo '   <table style="border: none">'.PHP_EOL;
		echo '       <tr style="border: none">'.PHP_EOL;
		echo '           <td style="border: none">File Name:</td>'.PHP_EOL;
		echo '           <td style="border: none">'.PHP_EOL;
		echo '              <input type="text" name=file_name value="'.get_new_filename().'"/>'.PHP_EOL;
		echo '           </td>'.PHP_EOL;
		echo '           <td style="border: none">  Size:</td>'.PHP_EOL;
		echo '           <td style="border: none">'.PHP_EOL;
		echo '              <input type="number" name=size value="10""/>'.PHP_EOL;
		echo '           </td>'.PHP_EOL;
		echo '           <td style="border: none">MB</td>'.PHP_EOL;
		echo '           <td style="border: none">'.PHP_EOL;
		echo '              <input type="submit" name="create" value="Create New" />'.PHP_EOL;
		echo '           </td>'.PHP_EOL;
		echo '      </tr>'.PHP_EOL;
		echo '   </table>'.PHP_EOL;
		echo '</form>'.PHP_EOL;
		echo '<br><i>Note: Creating a large file may take a long time!</i>'.PHP_EOL;
		echo '<br><br>'.PHP_EOL;
		echo '<form action="rascsi.php" method="post">'.PHP_EOL;
		echo '   <input type="submit" name="cancel" value="Cancel" />'.PHP_EOL;
		echo '</form>'.PHP_EOL;
	}
}

function action_delete_file(){
	// Check to see if the user has confirmed
	if(isset($_POST['confirmed'])){
		$command = 'rm '.$GLOBALS['FILE_PATH'].'/'.$_POST['file_name'];
		exec($command, $retArray, $result);
		check_result($result, $command, $retArray);
		html_generate_ok_to_go_home();
	}
	else{
		check_are_you_sure('Are you sure you want to PERMANENTLY delete '.$_POST['file_name'].'?');
	}
}

function action_restart_rascsi_service(){
	// Restart the RaSCSI service
	$command = "sudo /bin/systemctl restart rascsi.service 2>&1";
	exec($command, $retArray, $result);
	check_result($result, $command,$retArray);
	html_generate_ok_to_go_home();
}

function action_stop_rascsi_service(){
	// Stop the RaSCSI service
	$command = "sudo /bin/systemctl stop rascsi.service 2>&1";
	exec($command, $retArray, $result);
	check_result($result, $command,$retArray);
	html_generate_ok_to_go_home();
}

function action_reboot_raspberry_pi(){
	// Check to see if the user has confirmed
	if(isset($_POST['confirmed'])){
      $command = "sleep 2 && sudo reboot 2>&1";
      exec($command, $retArray, $result);
      // The unit should reboot at this point. Doesn't matter what we do now...
      check_result($result, $command,$retArray);
		html_generate_ok_to_go_home();
	}
	else{
		check_are_you_sure("Are you sure you want to reboot the Raspberry Pi?");
	}
}

function action_shutdown_raspberry_pi(){
	// Check to see if the user has confirmed
	if(isset($_POST['confirmed'])){
      $command = "sleep 2 && sudo shutdown -h now 2>&1";
      exec($command, $retArray, $result);
      // The unit should shutdown at this point. Doesn't matter what we do now...
      check_result($result, $command,$retArray);
		html_generate_ok_to_go_home();
	}
	else{
		check_are_you_sure("Are you sure you want to shut down the Raspberry Pi?");
	}
}

function action_unknown_command(){
	html_generate_warning('<br><h2>Unknown command: '.$_POST['command'].'</h2>');
	html_generate_ok_to_go_home();
}

function check_result($result,$command,$output){
	if(!$result){
		html_generate_success_message('Command succeeded!');
	}
	else{
      html_generate_warning('Command failed!');
	}
	echo '<br><code>'.$command.'</code><br>'.PHP_EOL;
	if(count($output) > 0){
		echo '<br>Output:<code>'.PHP_EOL;
		foreach($output as $line){
			echo '<br> Error message: '.$line.PHP_EOL;
		}
		echo '</code>'.PHP_EOL;
	}
}

function check_are_you_sure($prompt){
	echo '<br><h2>'.$prompt.'</h2>'.PHP_EOL;
	echo '      <table style="border: none">'.PHP_EOL;
	echo '      <tr style="border: none">'.PHP_EOL;
	echo '      <td style="border: none; vertical-align:top;">'.PHP_EOL;
	echo '      	<form action="rascsi_action.php" method="post">'.PHP_EOL;
	foreach($_POST as $key => $value){
		echo '<input type="hidden" name="'.$key.'" value="'.$value.'"/>'.PHP_EOL;
	}
	echo '      		<input type="hidden" name="confirmed" value="yes" />'.PHP_EOL;
	echo '      		<input type="submit" name="do_it" value="Yes" />'.PHP_EOL;
	echo '      	</form>'.PHP_EOL;
	echo '      </td>'.PHP_EOL;
	echo '    	<td style="border: none; vertical-align:top;">'.PHP_EOL;
	echo '      	<form action="rascsi.php" method="post">'.PHP_EOL;
	echo '      		<input type="submit" name="cancel" value="Cancel" />'.PHP_EOL;
	echo '      	</form>'.PHP_EOL;
	echo '      </td>'.PHP_EOL;
	echo '   </tr>'.PHP_EOL;
	echo '</table>'.PHP_EOL;
}

function action_connect_new_device(){
	// If we already know the type & filename, we can go connect the device...
	if(isset($_POST['type']) && isset($_POST['file_name'])){
		$command = 'rasctl -i '.$_POST['id'].' -c attach -t '.type_string_to_rasctl_type($_POST['type']);
		if($_POST['file_name'] != "None"){
			$command = $command.' -f '.$GLOBALS['FILE_PATH'].'/'.$_POST['file_name'];
		}
		exec($command, $retArray, $result);
		check_result($result, $command, $retArray);
		html_generate_ok_to_go_home();
	}
	else{


	$id = $_POST['id'];
		echo '<h2>Add New Device</h2>'.PHP_EOL;
		echo '<form action=rascsi_action.php method="post">'.PHP_EOL;
		echo '   <input type="hidden" name="command" value="'.$_POST['command'].'"/>'.PHP_EOL;
		echo '   <table style="border: none">'.PHP_EOL;
		echo '       <tr style="border: none">'.PHP_EOL;
		echo '           <td style="border: none">SCSI ID:</td>'.PHP_EOL;
		echo '           <td style="border: none">'.PHP_EOL;
		echo '           <input type="hidden" name=id value="'.$id.'"/>'.PHP_EOL;
		echo $id;
		echo '           </td>'.PHP_EOL;
		echo '           <td style="border: none">Device:</td>'.PHP_EOL;
		echo '           <td style="border: none">'.PHP_EOL;
		html_generate_scsi_type_select_list();
		echo '           </td>'.PHP_EOL;
		echo '           <td style="border: none">File:</td>'.PHP_EOL;
		echo '           <td style="border: none">'.PHP_EOL;
		echo '               <select name="file_name">'.PHP_EOL;
		echo '                  <option value="None">None</option>'.PHP_EOL;
      html_generate_image_file_select_list();
		echo '             </select>'.PHP_EOL;
		echo '          </td>'.PHP_EOL;
		echo '          <td style="border: none">'.PHP_EOL;
		echo '               <INPUT type="submit" value="Add"/>'.PHP_EOL;
		echo '          </td>'.PHP_EOL;
		echo '       </tr>'.PHP_EOL;
		echo '   </table>'.PHP_EOL;
	}
}

function get_new_filename(){
	// Try to find a new file name that doesn't exist.
	$i=1;
	while(file_exists($GLOBALS['FILE_PATH'].'/'.'new_file'.$i.'.hda'))
	{
		$i = $i+1;
	}
	return 'new_file'.$i.'.hda';
}

?>

</body>

</html>
