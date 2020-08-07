<!--  PHP source code for controlling the RaSCSI - 68kmla edition with a web interface. -->
<!--  Copyright (c) 2020 akuker -->
<!--  Distributed under the BSD-3 Clause License -->

<!-- Note: the origina rascsi-php project was under MIT license.-->

<?php

$DEBUG_ENABLE=0;
$FILE_PATH='/home/pi/images';
// Limit the maximum upload file size to 1GB
$MAX_UPLOAD_FILE_SIZE=1000000000;
$ALLOWED_FILE_TYPES=array('iso','hda');

function html_generate_header(){
	echo '    <table width="100%" >'. PHP_EOL;
	echo '        <tr style="background-color: black;">'. PHP_EOL;
	echo '          <td style="background-color: black;"><a href=http://github.com/akuker/RASCSI><h1>RaSCSI - 68kmla Edition</h1></a></td>'. PHP_EOL;
	echo '          <td style="background-color: black;">'. PHP_EOL;
	echo '                <form action="rascsi.php">'. PHP_EOL;
   echo '                    <input type="submit" value="Go Home"/>'. PHP_EOL;
   if($GLOBALS['DEBUG_ENABLE']){
      echo '                    <p style="color:#595959">Debug Timestamp: '.time().'</p>'. PHP_EOL;
   }
	echo '                </form>'. PHP_EOL;
	echo '          </td>'. PHP_EOL;
	echo '        </tr>'. PHP_EOL;
	echo '    </table>'. PHP_EOL;
	//echo(exec('whoami'));
}

function html_generate_image_file_select_list(){
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

         echo '<option value="'.$file_name.'">'.$file_name.'</option>'.PHP_EOL;
   }
}


function html_generate_scsi_id_select_list(){
	echo '<select>'. PHP_EOL;
	foreach(range(0,7) as $id){
		echo '<option value="'.$id.'">'.$id.'</option>'. PHP_EOL;
	}
	echo '</select>'. PHP_EOL;
}

function html_generate_scsi_type_select_list(){
	echo '<select name=type>'. PHP_EOL;
	$options = array("Hard Disk", "CD-ROM", "Zip Drive", "Ethernet Tap", "Filesystem Bridge");
	foreach($options as $type){
		echo '<option value="'.$type.'">'.$type.'</option>'. PHP_EOL;
	}
	echo '</select>'. PHP_EOL;
}

function html_generate_warning($message){
	echo '    <table width="100%" >'. PHP_EOL;
	echo '        <tr style="background-color: red;">'. PHP_EOL;
	echo '          <td style="background-color: red;">'. PHP_EOL;
	echo '              <font size=+2>'.$message.'</font>'. PHP_EOL;
	echo '         </td>'. PHP_EOL;
	echo '        </tr>'. PHP_EOL;
	echo '    </table>'. PHP_EOL;
}

function html_generate_success_message($message){
	echo '    <table width="100%" >'. PHP_EOL;
	echo '        <tr style="background-color: green;">'. PHP_EOL;
	echo '          <td style="background-color: green;">'. PHP_EOL;
   echo '              <font size=+2>Success</font>'. PHP_EOL;
	echo '         </td>'. PHP_EOL;
   echo '        </tr>'. PHP_EOL;
   if(strlen($message) > 0){
      echo '        <tr style="background-color: green;">'. PHP_EOL;
      echo '          <td style="background-color: green;">'. PHP_EOL;
      echo '              '.$message.PHP_EOL;
      echo '         </td>'. PHP_EOL;
      echo '        </tr>'. PHP_EOL;
   }
	echo '    </table>'. PHP_EOL;
}

function html_generate_ok_to_go_home(){
	echo '   <form action="rascsi.php">'. PHP_EOL;
	echo '       <input type="submit" value="OK"/>'. PHP_EOL;
	echo '   </form>'. PHP_EOL;
}


function current_rascsi_config() {
	$raw_output = shell_exec("/usr/local/bin/rasctl -l");
	$rasctl_lines = explode(PHP_EOL, $raw_output);

	echo '      <br>'. PHP_EOL;
	echo '      <h2>Current RaSCSI Configuration</h2>'. PHP_EOL;
	echo '      <table border="black">'. PHP_EOL;
	echo '          <tr>'. PHP_EOL;
	echo '              <td><b>SCSI ID</b></td>'. PHP_EOL;
	echo ' 	             <td><b>Type</b></td>'. PHP_EOL;
	echo '             <td><b>File</b></td>'. PHP_EOL;
	echo '              <td><b>File Ops</b></td>'. PHP_EOL;
	echo '              <td><b>Device Ops</b></td>'. PHP_EOL;
	echo '          </tr>'. PHP_EOL;

        $scsi_ids = array();

        foreach ($rasctl_lines as $current_line)
        {
                if(strlen($current_line) === 0){
                        continue;
                }
                if(strpos($current_line, '+----') === 0){
                        continue;

                }
                if(strpos($current_line, '| ID | UN') === 0){
                        continue;
                }
                $segments = explode("|", $current_line);

                $id_config = array();
                $id_config['id'] = trim($segments[1]);
                $id_config['type'] = trim($segments[3]);
                $id_config['file'] = trim($segments[4]);

                $scsi_ids[$id_config['id']] = $id_config;
        }


        foreach (range(0,7) as $id){
               echo '         <tr>'. PHP_EOL;
               echo '                 <td style="text-align:center">'.$id.'</td>'. PHP_EOL;
               if(isset($scsi_ids[$id]))
               {
                  echo '    <td style="text-align:center">'.$scsi_ids[$id]['type'].'</td>'. PHP_EOL;
                  if(strtolower($scsi_ids[$id]['file']) == "no media"){
                     echo '   <td>'.PHP_EOL;
                     echo '      <form action="rascsi_action.php" method="post">'. PHP_EOL;
                     echo '         <select name="file_name">'.PHP_EOL;
                     echo '            <option value="None">None</option>'.PHP_EOL;
                     html_generate_image_file_select_list();
                     echo '         </select>'.PHP_EOL;
                     echo '         <input type="hidden" name="command" value="insert_disk" />'. PHP_EOL;
                     echo '         <input type="hidden" name="id" value="'.$id.'" />'. PHP_EOL;
                     echo '         <input type="hidden" name="file" value="'.$scsi_ids[$id]['file'].'" />'. PHP_EOL;
                     echo '   </td><td>'.PHP_EOL;
                     echo '         <input type="submit" name="insert_disk" value="Insert" />'. PHP_EOL;
                     echo '      </form>'. PHP_EOL;
                     echo '   </td>'.PHP_EOL;
                  }
                  else{
                     // rascsi inserts "WRITEPROTECT" for the read-only drives. We want to display that differently.
                     echo '   <form action="rascsi_action.php" method="post">'. PHP_EOL;
                     echo '      <td>'.str_replace('(WRITEPROTECT)', '', $scsi_ids[$id]['file']). PHP_EOL;
                     echo '   </td><td>'.PHP_EOL;
                     if(strtolower($scsi_ids[$id]['type']) == 'sccd'){
                        echo '          <input type="hidden" name="command" value="eject_disk" />'. PHP_EOL;
                        echo '          <input type="hidden" name="id" value="'.$id.'" />'. PHP_EOL;
                        echo '          <input type="hidden" name="file" value="'.$scsi_ids[$id]['file'].'" />'. PHP_EOL;
                        echo '          <input type="submit" name="eject_disk" value="Eject" />'. PHP_EOL;
                     }
                     echo '   </td>'.PHP_EOL;
                     echo '      </form>'. PHP_EOL;
                  }
                  echo '    <td>'. PHP_EOL;
                  echo '       <form action="rascsi_action.php" method="post">'. PHP_EOL;
                  echo '          <input type="hidden" name="command" value="remove_device" />'. PHP_EOL;
                  echo '          <input type="hidden" name="id" value="'.$id.'" />'. PHP_EOL;
                  echo '          <input type="submit" name="remove_device" value="Disconnect" />'. PHP_EOL;
						echo '      </form>'. PHP_EOL;
                  echo '   </td>'. PHP_EOL;
                }
                else
                {
                  echo '                 <td style="text-align:center">-</td>'. PHP_EOL;
                  echo '                 <td>-</td>'. PHP_EOL;
                  echo '                 <td></td>'. PHP_EOL;
                  echo '                 <td>'. PHP_EOL;
						echo '                 <form action="rascsi_action.php" method="post">'. PHP_EOL;
						echo '                 <input type="hidden" name="command" value="connect_new_device" />'. PHP_EOL;
						echo '                 <input type="hidden" name="id" value="'.$id.'" />'. PHP_EOL;
						echo '                 <input type="submit" name="connect_new_device" value="Connect New" />'. PHP_EOL;
						echo '                 </form>'. PHP_EOL;
                        echo '                 </td>'. PHP_EOL;

                }
                echo '             </form>'. PHP_EOL;
                echo '         </tr>'. PHP_EOL;
	}
	echo '</table>'. PHP_EOL;
}
function get_all_files()
{
	$raw_ls_output = shell_exec('ls --time-style="+\"%Y-%m-%d %H:%M:%S\"" -alh --quoting-style=c '.$GLOBALS['FILE_PATH']);
	return $raw_ls_output;
}

function mod_date_from_ls($value){
	$ls_pieces = explode("\"", $value);
	if(count($ls_pieces)<1){
		return "";
	}
	return $ls_pieces[1];
}
function file_name_from_ls($value){
	$ls_pieces = explode("\"", $value);
	if(count($ls_pieces) < 4){
		return "";
	}
	return $ls_pieces[3];
}
function file_size_from_ls($value){
	$ls_pieces = explode("\"", $value);
	$file_props = preg_split("/\s+/", $ls_pieces[0]);
	return $file_props[4];
}
function file_category_from_file_name($value){
	if(strpos($value,".iso") > 0){
		return "CD-ROM Image";
	}
	if(strpos($value,".hda") > 0){
		return "Hard Disk Image";
	}
	return "Unknown type: " . $value;
}



function type_string_to_rasctl_type($typestr){
	if(strcasecmp($typestr,"Hard Disk") == 0){
		return "hd";
	}
	if(strcasecmp($typestr,"CD-ROM") == 0){
		return "cd";
	}
	if(strcasecmp($typestr,"Zip Drive") == 0){
	}
	if(strcasecmp($typestr,"Filesystem bridge") == 0){
		return "bridge";
	}
	return "";
}




?>
