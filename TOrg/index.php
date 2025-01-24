<?php
	require 'db.php';

	// Unique trust organization id
	$trustOrgId = "123456ABCDEF12A1";

	$bin_response = "";
	$inTrustOrgMode = false;
    $default_block2data_salt = "thayu!🥸";
    $default_trustkey_salt = "The only thing we have to fear is fear itself!🫣";

	function md5hash($data) {
        return hash("md5", strtoupper($data));
    }

	function sha256hash($data) {
        return hash("sha256", strtoupper($data));
    }

	function insertTrustKey ($new_block2data, $new_trustkey, $deviceUid, $secretKeyId) {
        try {
            global $con;
            global $trustOrgId;

            $sql = "INSERT INTO `rollingPasswordTable` (hashed_blockdata, rolling_pass, secret_key_id) ".
                        "VALUES('%s', '%s', '%d')";
            $sql = sprintf($sql, $new_block2data, $new_trustkey, $secretKeyId);
            //echo $sql;

            if (mysqli_query($con, $sql)) {
                // new Trust Key Will be:
                return  hextobin($new_trustkey) . hextobin($trustOrgId) . hextobin($deviceUid);
            }
        } catch (Exception $e) {
            //echo $e;
        }
        return "";
    };

	function findDevice($PCD_uid) {
        $deviceExists = false;
        try {
            global $con;
            global $inTrustOrgMode;

            $query = "SELECT is_trust_org FROM `devicesTable` WHERE device_id='$PCD_uid'";
            $result = mysqli_query($con, $query);

            $deviceExists = ($result && mysqli_num_rows($result) > 0);

            if ($deviceExists) {
                $inTrustOrgMode = (mysqli_fetch_row($result)[0] == 1);
            }

            mysqli_free_result($result);
        } catch (Exception $e) {
            //echo $e;
        }
        return $deviceExists;
    }

	if ($_SERVER["REQUEST_METHOD"] == "POST") {
        // Handle POST request.

        $bin_input = file_get_contents('php://input');
        //$bin_input = hex2bin($bin_input); // For postman testing only.
        //echo "Raw data :" . bin2hex($bin_input). "\n";

        $PCD_uid = "";
        $hashed_tag_uid = "";

        // Data packing
        if (strlen($bin_input) >= 19) {
            $PCD_uid = bin2hex(substr($bin_input, 11, 8));
            $uid_size = hexdec(bin2hex(substr($bin_input, 0, 1)));
            $hashed_tag_uid = md5hash(bin2hex(substr($bin_input, 1, $uid_size)));
        }

        try {
            if (!findDevice($PCD_uid)) { // PCD provided doesn't exist terminate further progress.
                $bin_input = "";
                $bin_response = "Hacking Attempt!";
            }
            //echo "content length: " . $_SERVER["CONTENT_LENGTH"]. "\n";
            //echo "string length: " . strlen($bin_input). "\n";

            /* ------------------------------------------------------------- *
                API Content length for secret key authentication endpoint is 35 bytes.
            * ------------------------------------------------------------- */
            if ($_SERVER["CONTENT_LENGTH"] >= 35 && strlen($bin_input) == 35) {

                $default_block2data = md5hash($default_block2data_salt.$hashed_tag_uid);
                $query = "SELECT secret_key FROM `secretKeysTable` WHERE hashed_tag_uid='$hashed_tag_uid'";
                $result = mysqli_query($con,$query);

                if (mysqli_num_rows($result) > 0) {
                    $bin_response = hex2bin(mysqli_fetch_row($result)[0]);
                }
                mysqli_free_result($result);
                //echo " Secret Key: ".bin2hex($bin_response). " \n";

                if (empty($bin_response) && $inTrustOrgMode) { // new card update by trust organization pcd.
                    $secret_key = substr(md5hash(random_bytes(8).$hashed_tag_uid), 0, 12);
                    $query = "INSERT INTO `secretKeysTable` (hashed_tag_uid, secret_key) VALUES('%s', '%s')";

                    $query = sprintf($query, $hashed_tag_uid, $secret_key);
                    if (mysqli_query($con, $query)) {
                            $bin_response = hex2bin($secret_key);
                    }

                    $secret_key_id = mysqli_insert_id($con);
                    $default_trustkey = sha256hash($default_trustkey_salt.$hashed_tag_uid)

                    // Also insert default trust key entry.
                    insertTrustKey($default_block2data, $default_trustkey, $PCD_uid, $secret_key_id);
                }

                if (!empty($bin_response)) { // Previous entry exists, validate block 2 data now.

                    $hashed_block2data = md5hash(bin2hex(substr($bin_input, 19, 16)));
                    //echo " -block2data :".$hashed_block2data. "\n";

                    $query = "SELECT id FROM `rollingPasswordTable` WHERE hashed_blockdata IN ('%s', '%s') ".
                            "ORDER BY created_on DESC LIMIT 1";
                    $query = sprintf($query, $hashed_block2data, $default_block2data);
                    $result = mysqli_query($con, $query);

                    //echo " Query: ".$query. " \n";

                    if (!$result || mysqli_num_rows($result) != 1){
                        $bin_response = ""; // Unexpected error occured
                    }

                    mysqli_free_result($result);
                } else {
                    // An error occured and the secret key couldn't be retrieved or it doesn't exist.
                    $bin_response = "Malformed request!-05";
                }
            }
            /* ------------------------------------------------------------- *
                API Content length for trust key authentication endpoint is 67
            * ------------------------------------------------------------- */
            elseif ($_SERVER["CONTENT_LENGTH"] >= 67 && strlen($bin_input) == 67) {

                // Validate if the full trust key matches the stored ones.
                $old_trustkey = bin2hex(substr($bin_input, 19, 32));
                $old_block2data  = md5hash(bin2hex(substr($bin_input, 51, 16)));
                $default_block2data = md5hash($default_block2data_salt.$hashed_tag_uid);
                $default_trustkey = sha256hash($default_trustkey_salt.$hashed_tag_uid)
                //echo " --block2data :".$old_block2data. "\n";

                // picks only the most recent trust key insert for authentication.
                $query = "SELECT t.s FROM (".
                			"SELECT `secret_key_id` as s, `rolling_pass` as r  from `rollingPasswordTable` as m ".
                        	"WHERE m.hashed_blockdata IN ('%s', '%s') ORDER BY m.created_on DESC LIMIT 1".
                        ") as t WHERE t.r IN ('%s', '%s')";
                $query = sprintf($query, $old_block2data, $default_block2data, $default_trustkey, $old_trustkey);
                $result = mysqli_query($con, $query);
                //echo $query . " \n";

                $secret_key_id = -1;
                if ($result && mysqli_num_rows($result) > 0) {
                    $secret_key_id = mysqli_fetch_row($result)[0];
                }

                mysqli_free_result($result);

                if($secret_key_id != -1 || $inTrustOrgMode) {
                    $new_block2data = md5hash($trustOrgId . $deviceUid);
                    $new_trustkey = sha256hash(random_bytes(8) . $old_trustkey);
                    $bin_response = insertTrustKey($new_block2data, $new_trustkey, $PCD_uid, $secret_key_id);
                }

                if (empty($bin_response)){
                    // An error occured and the secret key couldn't be retrieved or it doesn't exist.
                    $bin_response = "Malformed request!-06";
                }
            }
        } catch (Exception $e) {
            //echo $e;
        }

        //echo bin2hex($bin_response); //For postman testing only.
        echo $bin_response;
    } else {
        // Handle GET request.
        $page = 1;
        if (!empty($_GET) && !empty($_GET["page"])) {
            $page = max(1, intval($_GET["page"]));
        }

        $offset = ($page-1) * 10;

        try {
            $query = "SELECT hashed_blockdata, rolling_pass, created_on FROM `rollingPasswordTable` ".
                    "ORDER BY created_on DESC LIMIT 20 OFFSET ".$offset;
            $result = mysqli_query($con, $query);
            $data = array();

            if ($result && mysqli_num_rows($result) > 0) {
                while($row = mysqli_fetch_assoc($result)) {
                     array_push($data, (object)[
                        "created_on" => $row["created_on"],
                        "hashed_block2data" => $row["hashed_blockdata"],
                        "rolling_password" => substr($row["rolling_pass"],0,10)."...".substr($row["rolling_pass"],-10),
                    ]);
                }
            }

            mysqli_free_result($result);
        } catch (Exception $e) {
            //echo $e;
        }

        $info = (object)[
              "currentpage" => $_SERVER["REQUEST_SCHEME"]."://".$_SERVER["HTTP_HOST"].$_SERVER["PHP_SELF"]."?page=".$page,
              "data" => $data,
        ];

        header("Content-Type:application/json");
        echo json_encode($info);
    }

    //echo $data;
    //echo json_encode($_SERVER);
    mysqli_close($con);
?>