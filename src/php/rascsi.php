<!--  PHP source code for controlling the RaSCSI - 68kmla edition with a web interface. -->
<!--  Copyright (c) 2020 akuker -->
<!--  Distributed under the BSD-3 Clause License -->

<!-- Note: the origina rascsi-php project was under MIT license.-->

<!DOCTYPE html>
<html>

<head>
   <title>RaSCSI Main Control Page</title>
   <link rel="stylesheet" href="rascsi_styles.css">
 </head>

<body>

    <?php

	include 'lib_rascsi.php';
	html_generate_header();
	current_rascsi_config();
?>

    <br>
    <h2>Image File Management</h2>
    <table border="black" cellpadding="3">
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


		echo '<tr>'.PHP_EOL;
        echo '    <td>SD Card</td>'.PHP_EOL;
        echo '    <td>'.$file_name.'</td>'.PHP_EOL;
        echo '    <td>'.file_size_from_ls($this_file).'</td>'.PHP_EOL;
        echo '    <td>'.file_category_from_file_name($file_name).'</td>'.PHP_EOL;
        echo '    <td>'.mod_date_from_ls($this_file).'</td>'.PHP_EOL;
        echo '    <td>'.PHP_EOL;
        echo '       <form action="rascsi_action.php" method="post">'.PHP_EOL;
        echo '            <input type="hidden" name="command" value="delete_file"/>'.PHP_EOL;
        echo '            <input type="hidden" name="file_name" value="'.$file_name.'"/>'.PHP_EOL;
        echo '            <input type="submit" value="Delete">'.PHP_EOL;
        echo '       </form>'.PHP_EOL;
        echo '    </td>'.PHP_EOL;
        echo '</tr>'.PHP_EOL;
	}
?>
    </table>


    <br>
    <br>
    <h2>Upload New Image File</h2>
    <form action="rascsi_upload.php" method="post" enctype="multipart/form-data">
        <table style="border: none">
            <tr style="border: none">
                <td style="border: none; vertical-align:top;">
                    <input type="file" id="filename" name="file_name"><br><br>
                </td>
                <td style="border: none; vertical-align:top;">
                    <input type="submit" value="Upload" name="submit">
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
