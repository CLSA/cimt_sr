<?php

$util_path = '/home/dean/files/repository/php_util/';
require_once($util_path . 'util.class.php');
require_once($util_path . 'database.class.php');
util::initialize();

// assumes file sizes are equal
function files_are_equal($a, $b)
{
  $ah = fopen($a, 'rb');
  $bh = fopen($b, 'rb');
  $result = true;
  while(!feof($ah))
  {
    if(fread($ah, 8192) != fread($bh, 8192))
    {
      $result = false;
      break;
    }
  }

  fclose($ah);
  fclose($bh);

  return $result;
}

function toNull($value)
{
  return ('NA'==$value || ''==$value || NULL===$value || 0===strlen($value)) ? 'NULL' : $value;
}

function read_csv($fileName, $combine = true)
{
  $data = array();
  $file = fopen($fileName,'r');
  if(false === $file)
  {
    util::out('file ' . $fileName . ' cannot be opened');
    die();
  }
  $line = NULL;
  $line_count = 0;
  $nlerr = 0;
  $first = true;
  $header = NULL;
  while(false !== ($line = fgets($file)))
  {
    $line_count++;
    if($first)
    {
      $header = explode('","',trim($line,"\n\"\t"));
      $first = false;
      //var_dump($header);
      continue;
    }
    $line1 = explode('","',trim($line,"\n\"\t"));

    if(count($header)!=count($line1))
    {
      util::out(count($header) . ' < > ' . count($line1));
      util::out('Error: line (' . $line_count . ') wrong number of elements ' . $line);
      die();
    }
    $data[] = $combine ? array_combine($header,$line1) : (is_array($line1)&&1==count($line1) ? current($line1) : $line1);
  }
  fclose($file);
  return $data;
}

function xml_attribute($object, $attribute)
{
  if(isset($object[$attribute]))
    return (string) $object[$attribute];
  else
    return false;
}

if(2 != count($argv))
{
  util::out('usage: get_cIMT_rating_data path_to_alder_config_xml');
  exit;
}

$configPath = $argv[1];
if(substr($configPath,-1)!='/') $configPath .= '/';
$configFile = $configPath . 'config.xml' ;

$simple_xml_obj = simplexml_load_file($configFile);

$group_xml = array(
  'Database'=>array(
    'Host'=>null,
    'Username'=>null,
    'Password'=>null,
    'Name'=>null),
  'Opal'=>array(
    'Host'=>null,
    'Port'=>null,
    'Username'=>null,
    'Password'=>null),
  'Path'=>array(
    'ImageData'=>null)
  );
foreach($group_xml as $section=>$keys)
{
  foreach($keys as $key=>$value)
  {
    $match = $simple_xml_obj->xpath('//' . $section );
    if(false === $match)
    {
      util::out('Error: cannot find configuration option ' . $section);
      die();
    }
    $group_xml[$section][$key] = current( $simple_xml_obj->xpath('//' . $section  . '/' . $key));
  }
}
foreach($group_xml as $section=>$keys)
  foreach($keys as $key=>$value)
    util::out($section . '/'. $key . ': ' . $value);

$waveName = array('BL'=>'Baseline','FU1'=>'Followup1');
$wave = 'BL';
$waveRank = 1;

$imageDataRoot = $group_xml['Path']['ImageData'] . '/';

$db = new database(
  $group_xml['Database']['Host'],
  $group_xml['Database']['Username'],
  $group_xml['Database']['Password'],
  $group_xml['Database']['Name']);

// get the user Id's of the specified raters
//
$sql =
  'SELECT DISTINCT User.Id as userid, User.Name as user '.
  'FROM User '.
  'JOIN UserHasModality ON UserHasModality.UserId=User.Id '.
  'JOIN Modality ON Modality.Id=UserHasModality.ModalityId '.
  'WHERE Modality.Name="Ultrasound" ' .
  'AND User.Expert=true '.
  'AND User.Name IN ("lisa","jemc","staceyp")';

$user_list = $db->get_all($sql);

// get the rating data for each user
//
$path_list = array();
$indexed_list = array();
$index=0;
foreach($user_list as $user_data)
{
  $userid = $user_data['userid'];

  util::out('getting db info for user ' . $user_data['user']);
  $sql =
  'SELECT '.
  'User.Name AS RATER, '.
  'IFNULL(Rating.Rating,"NA") AS RATING, '.
  'IFNULL(Rating.DerivedRating,"NA") AS DERIVED, '.
  'IFNULL(x.Code,"NONE") AS CODE, '.
  'Exam.Side AS SIDE, '.
  'Interview.UId AS UID, '.
  'Interview.VisitDate AS VISITDATE, '.
  'Wave.Name AS WAVE, '.
  'Site.Name AS SITE, '.
  'CONCAT(Interview.Id,"/",Exam.Id,"/",Image.Id) AS PATH '.
  'FROM Rating '.
  'JOIN Image ON Image.Id=Rating.ImageId '.
  'JOIN Exam ON Exam.Id=Image.ExamId '.
  'JOIN Interview ON Interview.Id=Exam.InterviewId '.
  'JOIN Wave ON Wave.Id=Interview.WaveId '.
  'JOIN Site ON Site.Id=Interview.SiteId '.
  'JOIN ScanType ON ScanType.Id=Exam.ScanTypeId '.
  'AND ScanType.WaveId=Wave.Id '.
  'JOIN Modality ON Modality.Id=ScanType.ModalityId '.
  'JOIN User ON User.Id=Rating.UserId '.
  'LEFT JOIN ('.
  '  SELECT '.
  '  GROUP_CONCAT(DISTINCT Code ORDER BY Code SEPARATOR ",") AS Code, '.
  '  GROUP_CONCAT(DISTINCT CodeType.Id ORDER BY CodeType.Id SEPARATOR ",") AS CodeID, '.
  '  ImageId '.
  '  FROM Code '.
  '  JOIN CodeType ON CodeType.Id=Code.CodeTypeId '.
  '  JOIN Image ON Image.Id=Code.ImageId '.
  '  JOIN User ON User.Id=Code.UserId '.
  '  WHERE Code.UserId='. $userid . ' '.
  '  GROUP BY Image.Id '.
  ' ) x ON x.ImageId=Rating.ImageId '.
  'WHERE User.Id='. $userid . ' '.
  'AND Modality.Name="Ultrasound" '.
  'AND Wave.Rank=' . $waveRank . ' '.
  'AND ScanType.Type="CarotidIntima" '.
  'AND Image.Dimensionality=2';

  $alder_list = $db->get_all($sql);
  $target = count($alder_list);
  $count = 1;
  util::out('processing rating paths for user ' . $user_data['user']);
  foreach($alder_list as $data)
  {
    $path = $data['PATH'];
    if(!array_key_exists($path,$path_list))
    {
      $path_list[$path] = array();
    }
    $path_list[$path][] = $index;
    $indexed_list[$index] = $data;
    $index++;
    util::show_status($count++,$target);
  } // end loop on current user rating data
} // end loop on users

// for each unique file, get the dicom tags of interest
//
$index = 1;
$num_path = count($path_list);
$tags    = array('PATIENTID'=>'0010,0020','STUDYTIME'=>'0008,0030','STUDYDATE'=>'0008,0020');
$default = array('PATIENTID'=>'NA','STUDYTIME'=>'NA','STUDYDATE'=>'NA');

foreach($path_list as $key=>$index_data)
// key is the file path, index data is an array of integer indexes into the rating data
{
  // check the dicom file
  $fileName = $imageDataRoot . $key . '.dcm';
  $tag_data = $default;
  if(file_exists($fileName))
  {
     foreach($tags as $tagName=>$tagValue)
     {
       $res = '';
       if(1===preg_match("/\[(.*?)\]/",
         trim(shell_exec('dcmdump +P "' . $tagValue . '" ' . $fileName), "\n\"\t"), $res))
       {
         $res = rtrim(ltrim(current($res),'['),']');
       }
       else
       {
         $res='NA';
       }
       $tag_data[$tagName]=$res;
     }
  }
  foreach($index_data as $id)
  {
    $indexed_list[$id] = array_merge($indexed_list[$id],$tag_data);
  }

  util::show_status($index++,$num_path);
}

// write out the data
//
$index = 1;
$num_rating = count($indexed_list);
util::out('number of rating data pieces; ' . $num_rating);
$outfile = 'output_cIMT_rating_data.csv';
$file = fopen($outfile,'w');
$header = array(
   'RATER','RATING','DERIVED','CODE','SIDE',
   'UID','VISITDATE','WAVE','SITE','PATH','PATIENTID','STUDYTIME','STUDYDATE');
$str = '"' . util::flatten($header,'","') . '"' . PHP_EOL;
fwrite($file,$str);
foreach($indexed_list as $data)
{
  $str = '"' .  util::flatten(array_values($data),'","') . '"' . PHP_EOL;
  fwrite($file,$str);
  util::show_status($index++,$num_rating);
}
fclose($file);

util::out('output written to:' . $outfile);

