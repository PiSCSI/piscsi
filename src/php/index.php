<?php
header( 'Expires: Thu, 01 Jan 1970 00:00:00 GMT' );
header( 'Last-Modified: '.gmdate( 'D, d M Y H:i:s' ).' GMT' );

// HTTP/1.1
header( 'Cache-Control: no-store, no-cache, must-revalidate' );
header( 'Cache-Control: post-check=0, pre-check=0', FALSE );

// HTTP/1.0
header( 'Pragma: no-cache' );

define("IMAGE_PATH", "/home/pi/");
define("PROCESS_PATH", "/usr/local/bin/");
define("PROCESS_NAME1", "rasctl");
define("PROCESS_NAME2", "rascsi");

// Initial Settings
$hdType = array("HDS", "HDN", "HDI", "NHD", "HDA");
$moType = array("MOS");
$cdType = array("ISO");
$path = IMAGE_PATH;

// parameter check
if(isset($_GET['shutdown'])){
    // Power Down
    exec("sudo shutdown now");
} else if(isset($_GET['reboot'])){
    // Reboot
    exec("sudo shutdown -r now");
} else if(isset($_GET['start'])){
    // Startup
    $command = "sudo ".PROCESS_PATH.PROCESS_NAME2." 2>/dev/null";
    exec($command . " > /dev/null &");
    // 1.0s 
    sleep(1);
    // Information Display
    infoOut();
} else if(isset($_GET['stop'])){
    // End
    $command = "sudo pkill ".PROCESS_NAME2;
    $output = array();
    $ret = null;
    exec($command, $output, $ret);
    // 0.5s 
    usleep(500000);
    // Information Display
    infoOut();
} else if(isset($_GET['param'])){
    $param = $_GET['param'];
    if($param[0] === 'mounth') {
        // ファイルマウント
        $path = $param[2];
        $command = PROCESS_PATH.PROCESS_NAME1.' -i '.$param[1].' -c attach -t hd -f '.$param[2].$param[3];
        $output = array();
        $ret = null;
        exec($command, $output, $ret);
        // File mount
        infoOut();
    } else if($param[0] === 'mountm') {
        // File Connection
        $path = $param[2];
        $command = PROCESS_PATH.PROCESS_NAME1.' -i '.$param[1].' -c attach -t mo -f '.$param[2].$param[3];
        $output = array();
        $ret = null;
        exec($command, $output, $ret);
        // Information Display
        infoOut();
    } else if($param[0] === 'mountc') {
        // File connection
        $path = $param[2];
        $command = PROCESS_PATH.PROCESS_NAME1.' -i '.$param[1].' -c attach -t cd -f '.$param[2].$param[3];
        $output = array();
        $ret = null;
        exec($command, $output, $ret);
        // Information Display
        infoOut();
    } else if($param[0] === 'umount') {
        // File disconnect
        $command = PROCESS_PATH.PROCESS_NAME1.' -i '.$param[1].' -c detatch -t hd';
        $output = array();
        $ret = null;
        exec($command, $output, $ret);
        // Information Display
        infoOut();
    } else if($param[0] === 'insertm') {
        // Insert file
        $command = PROCESS_PATH.PROCESS_NAME1.' -i '.$param[1].' -c insert -t mo -f '.$param[2].$param[3];
        $output = array();
        $ret = null;
        exec($command, $output, $ret);
        // Information Display
        infoOut();
    } else if($param[0] === 'insertc') {
        // Insert file
        $command = PROCESS_PATH.PROCESS_NAME1.' -i '.$param[1].' -c insert -t cd -f '.$param[2].$param[3];
        $output = array();
        $ret = null;
        exec($command, $output, $ret);
        // Information Display
        infoOut();
    } else if($param[0] === 'eject') {
        // Eject file
        $command = PROCESS_PATH.PROCESS_NAME1.' -i '.$param[1].' -c eject';
        $output = array();
        $ret = null;
        exec($command, $output, $ret);
        // Information Display
        infoOut();
    } else if($param[0] === 'dir') {
        // Directory display
        $pos = strrpos($param[1], "..");
        if($pos !== false) {
            $pos1 = strrpos($param[1], "/", -5);
            $path = substr($param[1], 0, $pos1)."/";
        } else {
            $path = $param[1];
        }
        // Data display
        dataOut($path, $hdType, $moType, $cdType);
    }
} else {
    echo '<!DOCTYPE html><html><head><meta charset="utf-8">';
    echo '<script type="text/javascript">';
    echo 'window.onload = function(){';
    echo '  chMode();';
    echo '};';
    echo 'chMode();';
    echo 'function chMode() {';
    echo '  var elements = document.getElementsByName(\'mode\');';
    echo '  for(var a=\'\',i = elements.length; i--;) {';
    echo '    if(elements[i].checked) {';
    echo '      a = elements[i].value;';
    echo '      break;';
    echo '    }';
    echo '  }';
    echo '  if(a === "mu") {';
    echo '    var toenable = document.getElementsByClassName(\'mu\');';
    echo '    for (var k in toenable) {';
    echo '      toenable[k].disabled = \'\';';
    echo '    }';
    echo '    var todisable = document.getElementsByClassName(\'ie\');';
    echo '    for (var k in todisable) {';
    echo '      todisable[k].disabled = \'disabled\';';
    echo '    }';
    echo '  } else {';
    echo '    var todisable = document.getElementsByClassName(\'mu\');';
    echo '    for (var k in todisable) {';
    echo '      todisable[k].disabled = \'disabled\';';
    echo '    }';
    echo '    var toenable = document.getElementsByClassName(\'ie\');';
    echo '    for (var k in toenable) {';
    echo '      toenable[k].disabled = \'\';';
    echo '    }';
    echo '  }';
    echo '};';
    echo '</script></head><body>';
    // Information Display
    infoOut();

    // SCSI Operation
    echo '<p><div>Scsi Remove/Eject</div>';
    scsiOut($path);

    // Data display
    echo '<p><div>Image Operations</div>';
    dataOut($path, $hdType, $moType, $cdType);

    // Start / Stop
//    echo '<p><div>RASCSI Start/Stop</div>';
//    rascsiStartStop();

    // Reboot / Power down
    echo '<p><div>Raspberry Pi Reboot / Power down</div>';
    raspiRebootShut();

}

// Information Display
function infoOut() {
    echo '<div id="info">';
    // Process confirmation
    echo '<p><div>Process status:';
    $result = exec("ps -aef | grep ".PROCESS_NAME2." | grep -v grep", $output);
    if(empty($output)) {
        echo 'Stopping</div>';
    } else {
        echo 'Starting</div>';
        $command = PROCESS_PATH.PROCESS_NAME1.' -l';
        $output = array();
        $ret = null;
        exec($command, $output, $ret);
        foreach ($output as $line){
            echo '<div>'.$line.'</div>';
        }
    }
    echo '</div>';
}

// SCSI Operation
function scsiOut() {
    echo '<input type="radio" name="mode" onclick="chMode()" value="mu" checked>Connect/Disconnect';
    echo '<input type="radio" name="mode" onclick="chMode()" value="ie">Insert/Eject';

    echo '<table border="0">';
    echo '<tr>';
    // SCSI Eject / Disconnect
    echo '<td>';
    echo '<select id="ejectSelect">';
    echo   '<option value="0">SCSI0</option>';
    echo   '<option value="1">SCSI1</option>';
    echo   '<option value="2">SCSI2</option>';
    echo   '<option value="3">SCSI3</option>';
    echo   '<option value="4">SCSI4</option>';
    echo   '<option value="5">SCSI5</option>';
    echo '</select>';
    echo '</td>';

    // SCSI Disconnect
    echo '<td>';
    echo '<input type="button" class="mu" value="Disconnect" onclick="';
    echo 'var req = new XMLHttpRequest();';
    echo 'req.onreadystatechange = function(){';
    echo '  if(req.readyState == 4){';
    echo '    if(req.status == 200){';
    echo '      document.getElementById(\'info\').innerHTML = req.responseText;';
    echo '    }';
    echo '  }';
    echo '};';
    echo 'req.open(\'GET\',\'index.php?param[]=umount&param[]=\'+document.getElementById(\'ejectSelect\').value,true);';
    echo 'req.send(null);"/>';
    echo '</td>';

    // SCSI Eject
    echo '<td>';
    echo '<input type="button" class="ie" value="Eject" onclick="';
    echo 'var req = new XMLHttpRequest();';
    echo 'req.onreadystatechange = function(){';
    echo '  if(req.readyState == 4){';
    echo '    if(req.status == 200){';
    echo '      document.getElementById(\'info\').innerHTML = req.responseText;';
    echo '    }';
    echo '  }';
    echo '};';
    echo 'req.open(\'GET\',\'index.php?param[]=eject&param[]=\'+document.getElementById(\'ejectSelect\').value,true);';
    echo 'req.send(null);"/>';
    echo '</td>';

    echo '</tr>';
    echo '</table>';
}

// Data display
function dataOut($path, $hdType, $moType, $cdType) {
    $array_dir = array();
    $array_file1 = array();
    $array_file2 = array();
    $array_file3 = array();
    // Folder Check
    if($dir = opendir($path)) {
        while(($file = readdir($dir)) !== FALSE) {
            $file_path = $path.$file;
            if(!is_file($file_path)) {
                if(($path === IMAGE_PATH) &&
                   ($file === '..')) {
                    continue;
                }
                //Show directory
                if($file !== '.') {
                    $array_dir[] = $file;
                }
            } else {
                //Show files
                $path_data = pathinfo($file);
                if(array_key_exists('extension',$path_data))
                {
                    $ext = strtoupper($path_data['extension']);
                    if(in_array($ext, $hdType)) {
                        $array_file1[] = $file;
                    }
                    if(in_array($ext, $moType)) {
                        $array_file2[] = $file;
                    }
                    if(in_array($ext, $cdType)) {
                        $array_file3[] = $file;
                    }
                }
            }
        }
        closedir($dir);

        sort($array_dir);
        sort($array_file1);
        sort($array_file2);
        sort($array_file3);

        echo '<table id="table" border="0">';
        foreach ($array_dir as $file) {
            //Show directory
            echo '<tr>';
            echo '<td>';
            echo '<input type="button" value="Change" onclick="';
            echo 'var req = new XMLHttpRequest();';
            echo 'req.onreadystatechange = function(){';
            echo '  if(req.readyState == 4){';
            echo '    if(req.status == 200){';
            echo '      document.getElementById(\'table\').innerHTML = req.responseText;';
            echo '      chMode();';
            echo '    }';
            echo '  }';
            echo '};';
            echo 'req.open(\'GET\',\'index.php?param[]=dir&param[]='.$path.$file.'/'.'\',true);';
            echo 'req.send(null);"/>';
            echo '</td>';
            echo '<td>';
            echo '</td>';
            echo '<td>';
            echo '<div>'.$file.'</div>';
            echo '</td>';
            echo '</tr>';
        }
        $cnt = 0;
        foreach ($array_file1 as $file) {
            $cnt++;
            echo '<tr>';
            echo '<td>';
            echo '<select id="insertSelect'.$cnt.'">';
            echo   '<option value="0">SCSI0</option>';
            echo   '<option value="1">SCSI1</option>';
            echo   '<option value="2">SCSI2</option>';
            echo   '<option value="3">SCSI3</option>';
            echo   '<option value="4">SCSI4</option>';
            echo   '<option value="5">SCSI5</option>';
            echo '</select>';
            echo '</td>';

            echo '<td>';
            echo '<input type="button" class="mu" value="Connect" onclick="';
            echo 'var req = new XMLHttpRequest();';
            echo 'req.onreadystatechange = function(){';
            echo '  if(req.readyState == 4){';
            echo '    if(req.status == 200){';
            echo '      document.getElementById(\'info\').innerHTML = req.responseText;';
            echo '    }';
            echo '  }';
            echo '};';
            echo 'req.open(\'GET\',\'index.php?param[]=mounth&param[]=\'+document.getElementById(\'insertSelect'.$cnt.'\').value+\'&param[]='.$path.'&param[]='.$file.'\',true);';
            echo 'req.send(null);"/>';
            echo '</td>';

            echo '<td>';
            echo '<div>'.$file.'</div>';
            echo '</td>';
            echo '</tr>';
        }

        foreach ($array_file2 as $file) {
            $cnt++;
            echo '<tr>';
            echo '<td>';
            echo '<select id="insertSelect'.$cnt.'">';
            echo   '<option value="0">SCSI0</option>';
            echo   '<option value="1">SCSI1</option>';
            echo   '<option value="2">SCSI2</option>';
            echo   '<option value="3">SCSI3</option>';
            echo   '<option value="4">SCSI4</option>';
            echo   '<option value="5">SCSI5</option>';
            echo '</select>';
            echo '</td>';

            echo '<td>';
            echo '<input type="button" class="mu" value="Connect" onclick="';
            echo 'var req = new XMLHttpRequest();';
            echo 'req.onreadystatechange = function(){';
            echo '  if(req.readyState == 4){';
            echo '    if(req.status == 200){';
            echo '      document.getElementById(\'info\').innerHTML = req.responseText;';
            echo '    }';
            echo '  }';
            echo '};';
            echo 'req.open(\'GET\',\'index.php?param[]=mountm&param[]=\'+document.getElementById(\'insertSelect'.$cnt.'\').value+\'&param[]='.$path.'&param[]='.$file.'\',true);';
            echo 'req.send(null);"/>';
            echo '</td>';

            echo '<td>';
            echo '<input type="button" class="ie" value="Insert" onclick="';
            echo 'var req = new XMLHttpRequest();';
            echo 'req.onreadystatechange = function(){';
            echo '  if(req.readyState == 4){';
            echo '    if(req.status == 200){';
            echo '      document.getElementById(\'info\').innerHTML = req.responseText;';
            echo '    }';
            echo '  }';
            echo '};';
            echo 'req.open(\'GET\',\'index.php?param[]=insertm&param[]=\'+document.getElementById(\'insertSelect'.$cnt.'\').value+\'&param[]='.$path.'&param[]='.$file.'\',true);';
            echo 'req.send(null);"/>';
            echo '</td>';

            echo '<td>';
            echo '<div>'.$file.'</div>';
            echo '</td>';
            echo '</tr>';
        }

        foreach ($array_file3 as $file) {
            $cnt++;
            echo '<tr>';
            echo '<td>';
            echo '<select id="insertSelect'.$cnt.'">';
            echo   '<option value="0">SCSI0</option>';
            echo   '<option value="1">SCSI1</option>';
            echo   '<option value="2">SCSI2</option>';
            echo   '<option value="3">SCSI3</option>';
            echo   '<option value="4">SCSI4</option>';
            echo   '<option value="5">SCSI5</option>';
            echo '</select>';
            echo '</td>';

            echo '<td>';
            echo '<input type="button" class="mu" value="Connect" onclick="';
            echo 'var req = new XMLHttpRequest();';
            echo 'req.onreadystatechange = function(){';
            echo '  if(req.readyState == 4){';
            echo '    if(req.status == 200){';
            echo '      document.getElementById(\'info\').innerHTML = req.responseText;';
            echo '    }';
            echo '  }';
            echo '};';
            echo 'req.open(\'GET\',\'index.php?param[]=mountc&param[]=\'+document.getElementById(\'insertSelect'.$cnt.'\').value+\'&param[]='.$path.'&param[]='.$file.'\',true);';
            echo 'req.send(null);"/>';
            echo '</td>';

            echo '<td>';
            echo '<input type="button" class="ie" value="Insert" onclick="';
            echo 'var req = new XMLHttpRequest();';
            echo 'req.onreadystatechange = function(){';
            echo '  if(req.readyState == 4){';
            echo '    if(req.status == 200){';
            echo '      document.getElementById(\'info\').innerHTML = req.responseText;';
            echo '    }';
            echo '  }';
            echo '};';
            echo 'req.open(\'GET\',\'index.php?param[]=insertc&param[]=\'+document.getElementById(\'insertSelect'.$cnt.'\').value+\'&param[]='.$path.'&param[]='.$file.'\',true);';
            echo 'req.send(null);"/>';
            echo '</td>';

            echo '<td>';
            echo '<div>'.$file.'</div>';
            echo '</td>';
            echo '</tr>';
        }
        echo '</table>';
    }
}

// Start / Stop
function rascsiStartStop() {
    // Startup
    echo '<input type="button" value="起動" onclick="';
    echo 'var req = new XMLHttpRequest();';
    echo 'req.onreadystatechange = function(){';
    echo '  if(req.readyState == 4){';
    echo '    if(req.status == 200){';
    echo '      document.getElementById(\'info\').innerHTML = req.responseText;';
    echo '    }';
    echo '  }';
    echo '};';
    echo 'req.open(\'GET\',\'index.php?start=0\',true);';
    echo 'req.send(null);"/>';
    // Stop
    echo '<input type="button" value="停止" onclick="';
    echo 'var req = new XMLHttpRequest();';
    echo 'req.onreadystatechange = function(){';
    echo '  if(req.readyState == 4){';
    echo '    if(req.status == 200){';
    echo '      document.getElementById(\'info\').innerHTML = req.responseText;';
    echo '    }';
    echo '  }';
    echo '};';
    echo 'req.open(\'GET\',\'index.php?stop=0\',true);';
    echo 'req.send(null);"/>';
}

// Reboot / Power down
function raspiRebootShut() {
    // Reboot
    echo '<input type="button" value="Reboot" onclick="';
    echo 'var req = new XMLHttpRequest();';
    echo 'req.onreadystatechange = function(){';
    echo '  if(req.readyState == 4){';
    echo '    if(req.status == 200){';
    echo '      location.reload();';
    echo '    }';
    echo '  }';
    echo '};';
    echo 'req.open(\'GET\',\'index.php?reboot=0\',true);';
    echo 'req.send(null);"/>';
    // Power Down
    echo '<input type="button" value="Power off" onclick="';
    echo 'var req = new XMLHttpRequest();';
    echo 'req.onreadystatechange = function(){';
    echo '  if(req.readyState == 4){';
    echo '    if(req.status == 200){';
    echo '      location.reload();';
    echo '    }';
    echo '  }';
    echo '};';
    echo 'req.open(\'GET\',\'index.php?shutdown=0\',true);';
    echo 'req.send(null);"/>';

    echo '</body></html>';
}
?>
