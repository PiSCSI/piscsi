<!--  PHP source code for controlling the RaSCSI - 68kmla edition with a web interface. -->
<!--  Copyright (c) 2020 akuker -->
<!--  Distributed under the BSD-3 Clause License -->

<!-- Note: the origina rascsi-php project was under MIT license.-->

<?php

$FILE_PATH='/home/pi/images'. PHP_EOL;

function html_generate_header(){
	echo '    <table width="100%" >'. PHP_EOL;
	echo '        <tr style="background-color: black;">'. PHP_EOL;
	echo '          <td style="background-color: black;"><a href=http://github.com/akuker/RASCSI><h1>RaSCSI - 68kmla Edition</h1></a></td>'. PHP_EOL;
	echo '          <td style="background-color: black;">'. PHP_EOL;
	echo '                <form action="/rascsi.php">'. PHP_EOL;
	echo '                    <input type="submit" value="Refresh"/>'. PHP_EOL;
	echo '                    <p style="color:white">'.time().'</p>'. PHP_EOL;
	echo '                </form>'. PHP_EOL;
	echo '          </td>'. PHP_EOL;
	echo '        </tr>'. PHP_EOL;
	echo '    </table>'. PHP_EOL;
	//echo(exec('whoami'));
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
	echo '              <font size=+4>'.$message.'</font>'. PHP_EOL;
	echo '         </td>'. PHP_EOL;
	echo '        </tr>'. PHP_EOL;
	echo '    </table>'. PHP_EOL;
}

function html_generate_success_message(){
	echo '    <table width="100%" >'. PHP_EOL;
	echo '        <tr style="background-color: green;">'. PHP_EOL;
	echo '          <td style="background-color: green;">'. PHP_EOL;
	echo '              <font size=+2>Success!</font>'. PHP_EOL;
	echo '         </td>'. PHP_EOL;
	echo '        </tr>'. PHP_EOL;
	echo '    </table>'. PHP_EOL;
}

function html_generate_ok_to_go_home(){
	echo '   <form action="/rascsi.php">'. PHP_EOL;
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
	echo '             <td><b>Image File</b></td>'. PHP_EOL;
	echo '              <td><b>Actions</b></td>'. PHP_EOL;
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
                        echo '                 <td style="text-align:center">'.$scsi_ids[$id]['type'].'</td>'. PHP_EOL;
                        echo '                 <td>'.$scsi_ids[$id]['file'].'</td>'. PHP_EOL;
                        echo '                 <td>'. PHP_EOL;
						echo '                 <form action="rascsi_action.php" method="post">'. PHP_EOL;
						echo '                 <input type="hidden" name="command" value="eject" />'. PHP_EOL;
						echo '                 <input type="hidden" name="id" value="'.$id.'" />'. PHP_EOL;
						echo '                 <input type="hidden" name="file" value="'.$scsi_ids[$id]['file'].'" />'. PHP_EOL;
						echo '                 <input type="submit" name="eject" value="Eject" />'. PHP_EOL;
						echo '                 </form>'. PHP_EOL;
						echo '                 <form action="rascsi_action.php" method="post">'. PHP_EOL;
						echo '                 <input type="hidden" name="command" value="remove_device" />'. PHP_EOL;
						echo '                 <input type="hidden" name="id" value="'.$id.'" />'. PHP_EOL;
						echo '                 <input type="submit" name="remove_device" value="Disconnect" />'. PHP_EOL;
						echo '                 </form>'. PHP_EOL;
                        echo '                 </td>'. PHP_EOL;
                }
                else
                {
                        echo '                 <td style="text-align:center">-</td>'. PHP_EOL;
                        echo '                 <td>-</td>'. PHP_EOL;
                        echo '                 <td>'. PHP_EOL;
						echo '                 <form action="rascsi_action.php" method="post">'. PHP_EOL;
						echo '                 <input type="hidden" name="command" value="connect_new" />'. PHP_EOL;
						echo '                 <input type="hidden" name="id" value="'.$id.'" />'. PHP_EOL;
						echo '                 <input type="submit" name="connect_new" value="Connect New Device" />'. PHP_EOL;
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