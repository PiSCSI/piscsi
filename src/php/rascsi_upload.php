<!--  PHP source code for controlling the RaSCSI - 68kmla edition with a web interface. -->
<!--  Copyright (c) 2020 akuker -->
<!--  Distributed under the BSD-3 Clause License -->
<!DOCTYPE html>
<html>

<head>
   <title>RaSCSI Upload Page</title>
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
      echo '<br><br>'.PHP_EOL;
      var_dump($_FILES);
      echo '<br></p>'.PHP_EOL;
      echo '</td></tr></table>';
   }

   $target_dir = $GLOBALS['FILE_PATH'].'/';
   $target_file = $target_dir.basename($_FILES['file_name']['name']);
   $upload_ok=1;
   $file_type = strtolower(pathinfo($target_file,PATHINFO_EXTENSION));

	if(isset($_POST['submit']))
	{
      // Check if file already exists
      if ($upload_ok && (file_exists($target_file))) {
         html_generate_warning('Error: File '.$target_file.' already exists.');
         $upload_ok = 0;
      }

      // Check file size. Limit is specified in lib_rascsi.php
      if ($upload_ok && ($_FILES["file_name"]["size"] > $GLOBALS['MAX_UPLOAD_FILE_SIZE'])) {
         html_generate_warning("Error: your file is larger than the maximum size of " . $GLOBALS['MAX_UPLOAD_FILE_SIZE'] . "bytes");
         $upload_ok = 0;
      }

      // Allow certain file formats, also specified in lib_rascsi.php
      if($upload_ok && (!in_array(strtolower($file_type),$GLOBALS['ALLOWED_FILE_TYPES']))){
         $error_string = 'File type "'. $file_type. '" is not currently allowed.'.
         'Only the following file types are allowed: <br>'.
         '<ul>'.PHP_EOL;
         foreach($GLOBALS['ALLOWED_FILE_TYPES'] as $ft){
            $error_string = $error_string. '<li>'.$ft.'</li>'.PHP_EOL;
         }
         $error_string = $error_string.'</ul>';
         $error_string = $error_string.'<br>';
         html_generate_warning($error_string);
         $upload_ok = 0;
      }

   //Check if $upload_ok is set to 0 by an error
   if ($upload_ok != 0) {
      if (move_uploaded_file($_FILES["file_name"]["tmp_name"], $target_file)) {
         html_generate_success_message(basename( $_FILES["file_name"]["name"]). " has been uploaded.");
      } else {
         html_generate_warning("There was an unknown error uploading your file.");
      }
    }
   }
   else
   {
      html_generate_warning('The Submit POST information was not populated. Something went wrong');
   }
   echo '<br>';

   html_generate_ok_to_go_home();
?>

</body>

</html>
