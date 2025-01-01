<?php
		define ("SERVERNAME", "------");
        define ("USERNAME", "------");
        define ("PASSWORD", "------");

        define ("DBNAME", "------");

		mysqli_report(MYSQLI_REPORT_ERROR | MYSQLI_REPORT_STRICT);
        // Create connection
        $con = mysqli_connect(SERVERNAME, USERNAME, PASSWORD, DBNAME);
        
		// Check connection
        if (mysqli_connect_errno()){
 			echo "Failed to connect to MySQL: " . mysqli_connect_error();
 			die();
 		}
?>