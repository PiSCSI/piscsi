
<!--  PHP source code for controlling the RaSCSI - 68kmla edition with a web interface. -->
<!--  Copyright (c) 2020 akuker -->
<!--  Distributed under the BSD-3 Clause License -->

<!-- Note: the origina rascsi-php project was under MIT license.-->

<!DOCTYPE html>
<html>
    <head>
  <link rel="stylesheet" href="rascsi_styles.css">
<script>
function compute(f) {
	if (confirm("Are you sure?"))
		alert("Yes");
	else
		alert("No");
}

function eject_image(id,file){
	if(confirm("Not implemented yet.... would eject " + file + " from " + id))
		window.location = 'eject.php';
}
function insert_image(id,file){
	if(confirm("Not implemented yet.... would insert " + file + " into " + id))
		alert("OK");
}
function add_device(id){
	if(confirm("Not implemented yet.... would add device id: " + id))
		alert("OK");
}
function remove_device(id){
	if(confirm("Not implemented yet.... would remove device id: " + id))
		alert("OK");
}

function delete_file(f){
	if(confirm("Are you sure you want to delete "+f+"?"))
		alert("OK");
}

</script>
    </head>
    <body>

    <?php

	include 'lib_rascsi.php';
	html_generate_header();


       // parameter check
	if(isset($_GET['restart_rascsi_service'])){
           // Restart the RaSCSI service
           exec("sudo /bin/systemctl restart rascsi.service");
	} else if(isset($_GET['stop_rascsi_service'])){
	   // Stop the RaSCSI Service
           exec("sudo /bin/systemctl stop rascsi.service");
	} else if(isset($_GET['reboot_rasbperry_pi'])){
	   // Reboot the Raspberry Pi
           exec("sudo /sbin/reboot");
	} else if(isset($_GET['shutdown_raspberry_pi'])){
	   // Shut down the Raspberry Pi
	   echo "<h1>For now, shutdown is disabled....</h1>";
           echo 'exec("sudo /sbin/shutdown -s -t 0");';
	}

	current_rascsi_config();
?>

<br>
<h2>Add New Device</h2>
<form action=rascsi.php>
    <table style="border: none">
        <tr style="border: none">
            <td style="border: none">SCSI ID:</td>
            <td style="border: none">
<?php
	html_generate_scsi_id_select_list();
?>
            </td>
            <td style="border: none">Device:</td>
            <td style="border: none">
<?php
	html_generate_scsi_type_select_list();
?>
            </td>
            <td style="border: none">File:</td>
            <td style="border: none">
		<select>
<?php
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
?>
	      </select>
	   </td>
               <td style="border: none">
                    <INPUT type="submit" value="Add" onClick="add_device(1)"/>
                </td>
	</tr>
    </table>





    <br>

<form>
<input type=button value="asdf" onClick="compute(this.form)"><br>
</form>

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
            echo '            <input type="button" value="Delete" onClick="delete_file(\''.$file_name.'\')" data-arg1="'.$file_name.'"/>';
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
