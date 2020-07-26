<?php
header( 'Expires: Thu, 01 Jan 1970 00:00:00 GMT' );
header( 'Last-Modified: '.gmdate( 'D, d M Y H:i:s' ).' GMT' );

// HTTP/1.1
header( 'Cache-Control: no-store, no-cache, must-revalidate' );
header( 'Cache-Control: post-check=0, pre-check=0', FALSE );

// HTTP/1.0
header( 'Pragma: no-cache' );

define("IMAGE_PATH", "/home/pi/rasimg/");
define("PROCESS_PATH", "/usr/local/bin/");
define("PROCESS_NAME1", "rasctl");
define("PROCESS_NAME2", "rascsi");

// 初期設定
$hdType = array("HDS", "HDN", "HDI", "NHD", "HDA");
$moType = array("MOS");
$cdType = array("ISO");
$path = IMAGE_PATH;

// パラメタチェック
if(isset($_GET['shutdown'])){
    // 電源断
    exec("sudo shutdown now");
} else if(isset($_GET['reboot'])){
    // 再起動
    exec("sudo shutdown -r now");
} else if(isset($_GET['start'])){
    // 起動
    $command = "sudo ".PROCESS_PATH.PROCESS_NAME2." 2>/dev/null";
    exec($command . " > /dev/null &");
    // 1.0s 
    sleep(1);
    // 情報表示
    infoOut();
} else if(isset($_GET['stop'])){
    // 終了
    $command = "sudo pkill ".PROCESS_NAME2;
    $output = array();
    $ret = null;
    exec($command, $output, $ret);
    // 0.5s 
    usleep(500000);
    // 情報表示
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
        // 情報表示
        infoOut();
    } else if($param[0] === 'mountm') {
        // ファイル接続
        $path = $param[2];
        $command = PROCESS_PATH.PROCESS_NAME1.' -i '.$param[1].' -c attach -t mo -f '.$param[2].$param[3];
        $output = array();
        $ret = null;
        exec($command, $output, $ret);
        // 情報表示
        infoOut();
    } else if($param[0] === 'mountc') {
        // ファイル接続
        $path = $param[2];
        $command = PROCESS_PATH.PROCESS_NAME1.' -i '.$param[1].' -c attach -t cd -f '.$param[2].$param[3];
        $output = array();
        $ret = null;
        exec($command, $output, $ret);
        // 情報表示
        infoOut();
    } else if($param[0] === 'umount') {
        // ファイル切断
        $command = PROCESS_PATH.PROCESS_NAME1.' -i '.$param[1].' -c detatch -t hd';
        $output = array();
        $ret = null;
        exec($command, $output, $ret);
        // 情報表示
        infoOut();
    } else if($param[0] === 'insertm') {
        // ファイル挿入
        $command = PROCESS_PATH.PROCESS_NAME1.' -i '.$param[1].' -c insert -t mo -f '.$param[2].$param[3];
        $output = array();
        $ret = null;
        exec($command, $output, $ret);
        // 情報表示
        infoOut();
    } else if($param[0] === 'insertc') {
        // ファイル挿入
        $command = PROCESS_PATH.PROCESS_NAME1.' -i '.$param[1].' -c insert -t cd -f '.$param[2].$param[3];
        $output = array();
        $ret = null;
        exec($command, $output, $ret);
        // 情報表示
        infoOut();
    } else if($param[0] === 'eject') {
        // ファイル排出
        $command = PROCESS_PATH.PROCESS_NAME1.' -i '.$param[1].' -c eject';
        $output = array();
        $ret = null;
        exec($command, $output, $ret);
        // 情報表示
        infoOut();
    } else if($param[0] === 'dir') {
        // ディレクトリ表示
        $pos = strrpos($param[1], "..");
        if($pos !== false) {
            $pos1 = strrpos($param[1], "/", -5);
            $path = substr($param[1], 0, $pos1)."/";
        } else {
            $path = $param[1];
        }
        // データ表示
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
    // 情報表示
    infoOut();

    // SCSI操作
    echo '<p><div>Scsi取り外し/排出</div>';
    scsiOut($path);

    // データ表示
    echo '<p><div>Image操作</div>';
    dataOut($path, $hdType, $moType, $cdType);

    // 起動/停止
//    echo '<p><div>RASCSI 起動/停止</div>';
//    rascsiStartStop();

    // 再起動/電源断
    echo '<p><div>Raspberry Pi 再起動/電源断</div>';
    raspiRebootShut();

}

// 情報表示
function infoOut() {
    echo '<div id="info">';
    // プロセス確認
    echo '<p><div>プロセス状況:';
    $result = exec("ps -aef | grep ".PROCESS_NAME2." | grep -v grep", $output);
    if(empty($output)) {
        echo '停止中</div>';
    } else {
        echo '起動中</div>';
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

// SCSI操作
function scsiOut() {
    echo '<input type="radio" name="mode" onclick="chMode()" value="mu" checked>接続/切断';
    echo '<input type="radio" name="mode" onclick="chMode()" value="ie">挿入/排出';

    echo '<table border="0">';
    echo '<tr>';
    // SCSI?排出/切断
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

    // SCSI?切断
    echo '<td>';
    echo '<input type="button" class="mu" value="切断" onclick="';
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

    // SCSI?排出
    echo '<td>';
    echo '<input type="button" class="ie" value="排出" onclick="';
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

// データ表示
function dataOut($path, $hdType, $moType, $cdType) {
    $array_dir = array();
    $array_file1 = array();
    $array_file2 = array();
    $array_file3 = array();
    // フォルダチェック
    if($dir = opendir($path)) {
        while(($file = readdir($dir)) !== FALSE) {
            $file_path = $path.$file;
            if(!is_file($file_path)) {
                if(($path === IMAGE_PATH) &&
                   ($file === '..')) {
                    continue;
                }
                //ディレクトリを表示
                if($file !== '.') {
                    $array_dir[] = $file;
                }
            } else {
                //ファイルを表示
                $path_data = pathinfo($file);
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
        closedir($dir);

        sort($array_dir);
        sort($array_file1);
        sort($array_file2);
        sort($array_file3);

        echo '<table id="table" border="0">';
        foreach ($array_dir as $file) {
            //ディレクトリを表示
            echo '<tr>';
            echo '<td>';
            echo '<input type="button" value="変更" onclick="';
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
            echo '<input type="button" class="mu" value="接続" onclick="';
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
            echo '<input type="button" class="mu" value="接続" onclick="';
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
            echo '<input type="button" class="ie" value="挿入" onclick="';
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
            echo '<input type="button" class="mu" value="接続" onclick="';
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
            echo '<input type="button" class="ie" value="挿入" onclick="';
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

// 起動/停止
function rascsiStartStop() {
    // 起動
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
    // 停止
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

// 再起動/電源断
function raspiRebootShut() {
    // 再起動
    echo '<input type="button" value="再起動" onclick="';
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
    // 電源断
    echo '<input type="button" value="電源断" onclick="';
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
