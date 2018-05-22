<script language='javascript'>
window.setTimeout('window.location.reload()',1000);
</script>

<html>
<head>
    <title>Farm management</title>
</head>
<body>

<?php
$connect = mysqli_connect("localhost", "root", "root")
   or die("Connect Fail: " . mysqli_error());
echo "Connect Success!";
mysqli_select_db($connect,"test") or die("Select DB Fail! : " . mysqli_error());

$query = "select temp, light from cloud_farm";
$result = mysqli_query($connect, $query) or die("Query Fail: " . mysqli_error());
while ($row = mysqli_fetch_array($result)) {
    $entry .= "['".$row{'light'}."',".$row{'temp'}."],";
}

mysqli_free_result($result);

mysqli_close($connect);

?>

<div id="chart_div" style="width: 100%; height: 500px;"></div>
<script type="text/javascript" src="https://www.google.com/jsapi"></script>
<script type="text/javascript" src="http://ajax.googleapis.com/ajax/libs/jquery/1.8.2/jquery.min.js"></script>
<script type="text/javascript">
    google.load("visualization", "1", {packages:["corechart"]});
    google.setOnLoadCallback(drawChart);
    function drawChart() {
        var data = google.visualization.arrayToDataTable([
        ['bright',	'temp'],
        <?php echo $entry ?>
    ]);
        var options = {
            title: 'Farm management',
            curveType: 'function',
            legend: { position: 'bottom' }
        };
        var chart = new google.visualization.LineChart(document.getElementById('chart_div'));
        chart.draw(data, options);
    }
</script>
</body>
</html>
