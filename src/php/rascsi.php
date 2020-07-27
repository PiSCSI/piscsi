
<!--  PHP source code for controlling the RaSCSI - 68kmla edition with a web interface. -->
<!--  Copyright (c) 2020 akuker -->
<!--  Distributed under the BSD-3 Clause License -->

<!-- Note: the origina rascsi-php project was under MIT license.-->

<!DOCTYPE html>
<html>
    <head>

        <style>h1{
            color:white;
            font-size:20px;
            font-family: Arial, Helvetica, sans-serif;
            background-color: black;
        }</style>
        <style>h2{
            color:black;
            font-size:16px;
            font-family: Arial, Helvetica, sans-serif;
            background-color: white;
            margin: 0px;
        }</style>
        </style>
        <style>body{
            color:black;
            font-family:Arial, Helvetica, sans-serif;
            background-color: white;
        }</style>
        <STYLE>A {text-decoration: none;} </STYLE>
        <style>table,tr,td {
            border: 1px solid black;
            border-collapse:collapse;
            margin: none;
            font-family:Arial, Helvetica, sans-serif;
        }
        </style>
    </head>
    <body>
    <table width="100%" >
        <tr style="background-color: black;">
          <td style="background-color: black;"><a href=http://github.com/akuker/RASCSI><h1>RaSCSI - 68kmla Edition</h1></a></td>
	  <td style="background-color: black;">
		<form action="/rascsi.php">
		    <input type="submit" value="Refresh"/>
		</form>
	  </td>
	</tr>
    </table>

    <?php
       echo "Debug timestamp:";
       $t=time();
       echo($t . "<br>");
       // parameter check
	if(isset($_GET['restart_rascsi_service'])){
           // Restart the RaSCSI service
           echo 'exec("sudo systemctl restart rascsi.service");';
	} else if(isset($_GET['stop_rascsi_service'])){
	   // Stop the RaSCSI Service
           echo 'exec("sudo systemctl stop rascsi.service");';
	} else if(isset($_GET['reboot_rasbperry_pi'])){
	   // Reboot the Raspberry Pi
           echo 'exec("sudo shutdown -r -t 0");';
	} else if(isset($_GET['shutdown_raspberry_pi'])){
           // Shut down the Raspberry Pi 
           echo 'exec("sudo shutdown -s -t 0");';
	}

	current_rascsi_config();
	


function current_rascsi_config() {
	$raw_output = shell_exec("/usr/local/bin/rasctl -l");
	$rasctl_lines = explode(PHP_EOL, $raw_output);

	echo '      <br>';
	echo '      <h2>Current RaSCSI Configuration</h2>';
	echo '      <table border="black">';
	echo '          <tr>';
	echo '              <td><b>SCSI ID</b></td>';
	echo ' 	             <td><b>Type</b></td>';
	echo '             <td><b>Image File</b></td>';
	echo '              <td><b>Actions</b></td>';
	echo '          </tr>';
	
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
			echo 'Dont stop til you get enough';
		}
		$segments = explode("|", $current_line);

		echo '         <tr>';
		echo '             <form>';
		echo '                 <td>'.$segments[1].'</td>';
		echo '                 <td>'.$segments[3].'</td>';
		echo '                 <td>'.$segments[4].'</td>';
		echo '                 <td>';
		echo '                     <input type="button" value="Eject"/>';
		echo '                     <input type="button" value="Disconnect">';
		echo '                 </td>';
		echo '             </form>';
		echo '         </tr>';

	}
	echo '</table>';
}
function get_all_files()
{
	$raw_ls_output = shell_exec('ls --time-style="+\"%Y-%m-%d %H:%M:%S\"" -alh --quoting-style=c /home/pi/images/');
	return $raw_ls_output;
}

?>

<br>
<h2>Add New Device</h2>
<form action=rascsi.php>
    <table style="border: none">
        <tr style="border: none">
            <td style="border: none">SCSI ID:</td>
            <td style="border: none">
                <select>
                    <option value="0">0</option>
                    <option value="1">1</option>
                    <option value="2">2</option>
                    <option value="3">3</option>
                    <option value="4">4</option>
                    <option value="5">5</option>
                    <option value="6">6</option>
                    <option value="7">7</option>
                </select>
            </td>
            <td style="border: none">Device:</td>
            <td style="border: none">
                <select>
                    <option value="hard_disk">Hard Disk</option>
                    <option value="cd_rom">CD-ROM</option>
                    <option value="zip_drive" disabled>Zip Drive</option>
                    <option value="ethernet_tap" disabled>Ethernet Tap</option>
                    <option value="filesystem_bridge" disabled>Filesystem Bridge</option>
                </select>
            </td>
            <td style="border: none">File:</td>
            <td style="border: none">
		<select>
<?php
	$all_files = get_all_files();
	foreach(explode(PHP_EOL, $all_files) as $this_file){
		if(strpos($current_line, 'total') === 0){
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

function mod_date_from_ls($value){
	$ls_pieces = explode("\"", $value);
	return $ls_pieces[1];
}
function file_name_from_ls($value){
	$ls_pieces = explode("\"", $value);
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
?>
	      </select>
	   </td>
               <td style="border: none">
                    <INPUT type="submit" value="Add">
                </td>
	</tr>
    </table>





    <br>
    <h2>Image File Management</h2>
    <table border="black">
        <tr>
            <td><b>Location</b></td>
            <td><b>Filename</b></td>
            <td><b>Size</b></td>
            <td><b>Type</b></td>
            <td><b>Date Modified</b></td>
            <td><b>Actions</b></td>
        </tr>
	<tr>
<?php
	// Generate the table with all of the file names in it.....
	$all_files = get_all_files();
	foreach(explode(PHP_EOL, $all_files) as $this_file){
		if(strpos($this_file, 'total') === 0){
			continue;
		}
		$file_name = file_name_from_ls($this_file);
		if(strlen($file_name) === 0){
			continue;
		}
		// Ignore file names that start with .
		if(strpos($file_name,".") === 0){
			continue;
		}


		echo '<tr>';
    echo '    <form>';
        echo '        <td>SD Card</td>';
        echo '        <td>'.$file_name.'</td>';
        echo '        <td>'.file_size_from_ls($this_file).'</td>';
        echo '        <td>'.file_category_from_file_name($file_name).'</td>';
        echo '       <td>'.mod_date_from_ls($this_file).'</td>';
        echo '       <td>';
            echo '            <input type="button" value="Delete"/>';
            echo '            <input type="button" value="Copy to RAM Disk" disabled/>';
            echo '        </td>';
            echo '    </form>';
            echo '</tr>';
	}
?>
    </table>


    <br>
    <br>
    <h2>Upload New Image File</h2>
    <form>
        <table style="border: none">
            <tr style="border: none">
                <td style="border: none; vertical-align:top;">
                    <input type="file" id="filename" name="fname"><br><br>
                </td>
                <td style="border: none; vertical-align:top;">
                    <input type="submit" value="Upload">
                </td>
            </tr>
        </table>
    </form>

    <br>
    <h2>RaSCSI Service Status</h2>
    <form method="get" action="rascsi.php" id="service">
	<input type="submit" name="restart_rascsi_service" value="Restart RaSCSI service"/>
        <input type="submit" name="stop_rascsi_service" value="Stop RaSCSI service"/>
    </form>

    <br>
    <h2>Raspberry Pi Operations</h2>
    <form id="pi stuff">
        <input type="submit" name="reboot_rasbperry_pi" value="Reboot Raspberry Pi"/>
        <input type="submit" name="shutdown_raspberry_pi" value="Shut Down Raspberry Pi"/>
    </form>

   </body>
</html>
