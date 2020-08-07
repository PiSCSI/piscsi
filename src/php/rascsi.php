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

    function eject_image(id, file) {
        var url = "eject.php?id=" + encodeURIComponent(id) + "&file=" + encodeURIComponent(file);
        window.location.href = url;
    }

    function insert_image(id, file) {
        if (confirm("Not implemented yet.... would insert " + file + " into " + id))
            alert("OK");
    }

    function add_device(id) {
        var url = "add_device.php?id=" + encodeURIComponent(id);
        window.location.href = url;
    }

    function remove_device(id) {
        confirm_message = "Are you sure you want to disconnect ID " + id +
            "? This may cause unexpected behavior on the host computer if it is still running";
        if (confirm(confirm_message)) {
            var url = "disconnect.php?id=" + encodeURIComponent(id);
            window.location.href = url;
        }
    }

    function delete_file(f) {
        if (confirm("Are you sure you want to delete " + f + "?"))
            alert("OK");
    }
    </script>
</head>

<body>

    <?php

	include 'lib_rascsi.php';
	html_generate_header();
	current_rascsi_config();
?>

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
    <h2>Create New Empty HD Image</h2>
    <form action="rascsi_action.php" method="post">
        <input type="hidden" name="command" value="create_new_image" />
        <input type="submit" value="Create New">
    </form>

    <br>
    <h2>RaSCSI Service Status</h2>
    <table style="border: none">
            <tr style="border: none">
                <td style="border: none; vertical-align:top;">
                <form action="rascsi_action.php" method="post">
                    <input type="hidden" name="command" value="restart_rascsi_service" />
                    <input type="submit" name="restart_rascsi_service" value="Restart RaSCSI service" />
                </form>
            </td>
            <td style="border: none; vertical-align:top;">
                <form action="rascsi_action.php" method="post">
                    <input type="hidden" name="command" value="stop_rascsi_service" />
                    <input type="submit" name="stop_rascsi_service" value="Stop RaSCSI service" />
                </form>
            </td>
        </tr>
    </table>

    <br>
    <h2>Raspberry Pi Operations</h2>
    <table style="border: none">
            <tr style="border: none">
                <td style="border: none; vertical-align:top;">
                <form action="rascsi_action.php" method="post">
                    <input type="hidden" name="command" value="reboot_raspberry_pi" />
                    <input type="submit" name="reboot_rasbperry_pi" value="Reboot Raspberry Pi" />
                </form>
            </td>
            <td style="border: none; vertical-align:top;">
                <form action="rascsi_action.php" method="post">
                    <input type="hidden" name="command" value="shutdown_raspberry_pi" />
                    <input type="submit" name="shutdown_raspberry_pi" value="Shut Down Raspberry Pi" />
                </form>
            </td>
        </tr>
    </table>

</body>

</html>