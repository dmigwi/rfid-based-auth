<?php
	require 'db.php';

	// Unique trust organization id
	$trustOrgId = "123456ABCDEF12A1";
	$response = "";
	$inTrustOrgMode = false;
	$deviceExists = false;

	function md5hash($data) {
        return hash("md5", strtoupper($data));
    }

	function sha256hash($data) {
        return hash("sha256", strtoupper($data));
    }

    $default_trustkey = sha256hash("The only thing we have to fear is fear itself!🫣");
    $default_block2data = md5hash("thayu!🥸");

	function insertTrustKey ($new_trustkey, $deviceUid, $secretKeyId) {
        try {
            global $con;
            global $trustOrgId;
            global $default_block2data;
            global $default_trustkey;

            $new_block2data = $default_block2data;
            if ($new_trustkey != $default_trustkey) {
                $new_block2data = md5hash($trustOrgId . $deviceUid);
            }
            //echo " * new block2data :".$new_block2data. "\n";

            $sql = "INSERT INTO `rollingPasswordTable` (hashed_blockdata, rolling_pass, secret_key_id) VALUES('%s', '%s', '%d')";
            $sql = sprintf($sql, $new_block2data, $new_trustkey, $secretKeyId);
            //echo $sql;

            if (mysqli_query($con, $sql)) {
                // new Trust Key Will be:
                return $new_trustkey . $trustOrgId . $deviceUid;
            }
        } catch (Exception $e) {
            //echo $e;
        }
        return "";
    };

	function findDevice($device_uid) {
        try {
            global $con;
            global $inTrustOrgMode;
            global $deviceExists;

            $query = "SELECT is_trust_org FROM `devicesTable` WHERE device_id='$device_uid'";
            $result = mysqli_query($con, $query);

            $deviceExists = ($result && mysqli_num_rows($result) > 0);

            if ($deviceExists) {
                $inTrustOrgMode = (mysqli_fetch_row($result)[0] == 1);
            }

            mysqli_free_result($result);
        } catch (Exception $e) {
            //echo $e;
        }
    }

	if ($_SERVER["REQUEST_METHOD"] == "POST") {
        // Handle POST request.

        $data = file_get_contents('php://input');
        //$data = hex2bin($data); // For postman testing only.
        //echo "Raw data :" . bin2hex($data). "\n";

        $hashed_tag_uid = "";
        $device_uid = "";

        // Data packing
        if (strlen($data) >= 19) {
            $uid_size = hexdec(bin2hex(substr($data, 0, 1)));
            $hashed_tag_uid = md5hash(bin2hex(substr($data, 1, $uid_size)));
            $device_uid = bin2hex(substr($data, 11, 8));
        }

        try {
            findDevice($device_uid);
            //echo "content length: " . $_SERVER["CONTENT_LENGTH"]. "\n";
            //echo "string length: " . strlen($data). "\n";

            // Content length for secret key authentication is 35 (in Ascii format)
            if ($_SERVER["CONTENT_LENGTH"] >= 35 && strlen($data) == 35) {

                $query = "SELECT secret_key FROM `secretKeysTable` WHERE hashed_tag_uid='$hashed_tag_uid'";
                $result = mysqli_query($con,$query);

                if (mysqli_num_rows($result) > 0) {
                    $response = mysqli_fetch_row($result)[0];
                }
                mysqli_free_result($result);
                //echo " Secret Key: ".$response. " \n";


                if (empty($response) && $inTrustOrgMode) { // new card update by trust organization pcd.
                    $secret_key = substr(md5hash(random_bytes(8).$hashed_tag_uid), 0, 12);
                    $query = "INSERT INTO `secretKeysTable` (hashed_tag_uid, secret_key) VALUES('%s', '%s')";

                    $query = sprintf($query, $hashed_tag_uid, $secret_key);
                    if (mysqli_query($con, $query)) {
                            $response = $secret_key;
                    }

                    $secret_key_id = mysqli_insert_id($con);

                    // Also insert default trust key entry.
                    insertTrustKey($default_trustkey, $device_uid, $secret_key_id);
                }

                if (!empty($response)) { // Previous entry exists, validate block 2 data now.

                    $hashed_block2data = md5hash(bin2hex(substr($data, 19, 16)));
                    //echo " -block2data :".$hashed_block2data. "\n";

                    $query = "SELECT id FROM `rollingPasswordTable` WHERE hashed_blockdata IN ('%s', '%s') ".
                            "ORDER BY created_on DESC LIMIT 1";
                    $query = sprintf($query, $hashed_block2data, $default_block2data);
                    $result = mysqli_query($con, $query);

                    //echo " Query: ".$query. " \n";

                    if (!$result || mysqli_num_rows($result) != 1){
                        $response = ""; // Unexpected error occured
                    }

                    mysqli_free_result($result);
                } else {
                    // An error occured and the secret key couldn't be retrieved or it doesn't exist.
                    $response = bin2hex("Malformed request body found!-04");
                }
            }

            if ($_SERVER["CONTENT_LENGTH"] >= 67 && strlen($data) == 67) {
                // Content length for trust key authentication is 67

                $prevTrustKeyMatches = false;
                // Validate if the full trust key matches the stored ones.
                $old_trustkey = bin2hex(substr($data, 19, 32));
                $old_block2data  = md5hash(bin2hex(substr($data, 51, 16)));
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
                    $prevTrustKeyMatches = true;
                }

                mysqli_free_result($result);

                // Validate the device uid provided if it is among the supported.
                if($deviceExists && $secret_key_id != -1) {
                    if ($prevTrustKeyMatches || $inTrustOrgMode) {
                        $new_trustkey = sha256hash(random_bytes(8).$old_trustkey);
                        $response = insertTrustKey($new_trustkey, $device_uid, $secret_key_id);
                    }
                }

                if (empty($response)){
                    // An error occured and the secret key couldn't be retrieved or it doesn't exist.
                    $response = bin2hex("Malformed request body found!-05");
                }
            }
        } catch (Exception $e) {
            //echo $e;
        }

        //echo $response; //For postman testing only.
        echo hex2bin($response);
    } else {
        // Handle GET request.
        $page = 1;
        if (!empty($_GET) && !empty($_GET["page"])) {
            $page = max(1, intval($_GET["page"]));
        }

        $offset = ($page-1) * 10;

        try {
            $query = "SELECT hashed_blockdata, rolling_pass, created_on FROM rollingPasswordTable ORDER BY created_on DESC LIMIT 10 OFFSET ".$offset;
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