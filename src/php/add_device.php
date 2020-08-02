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

	// Check if we're passed an ID
        if(isset($_GET['id'])){
	   $id = $_GET['id'];
	}
	else {
		html_generate_warning('Page opened without arguments');
	}

	if(isset($_GET['type'])){
	   $type = type_string_to_rasctl_type($_GET['type']);
	   if(strlen($type) < 1){
		   html_generate_warning('Unknown drive type: '.$_GET['type']);
	   }

	   $cmd = 'rasctl -i '.$id.' -c attach -t '.$type;
	   
	   // Check to see if the file name is specified
	   if(isset($_GET['file'])){
		   if(strcasecmp($_GET['file'],"None") != 0)
		   {
			   $cmd = $cmd.' -f '.$FILE_PATH.'/'.$_GET['file'];
		   } 
	   }

	   $result = "Command not ran.....";
	   // Go do the actual action
	   if(strlen($type) > 0){
	   	$result = exec($cmd);
	   	echo '<br>';
	   	echo 'Ran command: <pre>'.$cmd.'</pre>';
	   	echo '<br>';
	   }
	   // Check to see if the command succeeded
           if(strlen($result) > 0){
		html_generate_warning($result);
	   }
	   else {
		html_generate_success_message();
	   }
	   echo '<br>';
	   html_generate_ok_to_go_home();
	}
	else {
	   html_generate_add_new_device(trim($id));
	}


function html_generate_add_new_device($id){
	echo '<h2>Add New Device</h2>';
	echo '<form action=add_device.php method="GET">';
 	echo '   <table style="border: none">';
 	echo '       <tr style="border: none">';
 	echo '           <td style="border: none">SCSI ID:</td>';
	echo '           <td style="border: none">';
	echo '           <input type="hidden" name=id value="'.$id.'"/>';
	echo $id;
 	echo '           </td>';
 	echo '           <td style="border: none">Device:</td>';
	echo '           <td style="border: none">';
        html_generate_scsi_type_select_list();
  	echo '           </td>';
  	echo '           <td style="border: none">File:</td>';
  	echo '           <td style="border: none">';
	echo '               <select name="file">';
	echo '                  <option value="None">None</option>';
        $all_files = get_all_files();
        foreach(explode(PHP_EOL, $all_files) as $this_file){
                if(strpos($this_file, 'total') === 0){
                        continue;
                }
                $file_name = file_name_from_ls($this_file);
                if(strlen($file_name) === 0){
                        continue;
                }
                // Ignore files that start with a .
                if(strpos($file_name, '.') === 0){
                        continue;
                }
                
                echo '<option value="'.$file_name.'">'.$file_name.'</option>';
        }
  	echo '             </select>';
  	echo '          </td>';
  	echo '          <td style="border: none">';
  	echo '               <INPUT type="submit" value="Add"/>';
  	echo '          </td>';
  	echo '       </tr>';
  	echo '   </table>';
}
?>

  </body>
</html>
