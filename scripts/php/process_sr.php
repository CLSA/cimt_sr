<?php

require_once('/home/dean/files/sandbox/php/util.class.php');
util::initialize();

// given an unmatching barcode, try to generate a valid substitution
//
function repair_barcode_id($id, $opal_id, $opal_id_list)
{
  if($id==$opal_id) return $id;
  $id_list = str_split($id);
  $id_length = count($id_list);
  $res = implode(array_intersect($opal_id_list,$id_list));
  $ndiff = count(array_diff($id_list, $opal_id_list));
  if($res == $opal_id && $id_length != 8)
  {
    return $res;
  }
  if($id_length == 7 && 0 == $ndiff)
  {
    // is it a substring of the true id
    if(false !== strpos($opal_id, $res))
    {
      return $opal_id;
    }
    else
    {
      $res2 = implode(array_intersect($id_list,$opal_id_list));
      // all of the digits can be found in the true id
      if($res2 == $res)
        return $opal_id;
    }
  }
  return false;
}

function read_csv($fileName)
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
    $line1 = array_combine($header,$line1);
    $data[] = $line1;
  }
  fclose($file);
  return $data;
}

function get_match_count($source,$target)
{
  $res = 0;
  if(!is_array($source))
    $source = str_split($source);
  if(!is_array($target))
    $target = str_split($target);
  $n = count($source);
  if($n==count($target))
  {
    for($i=0;$i<$n;$i++)
    {
      $j = $i+1;
      if($source[$i]===$target[$i] ||
         ($j < $n  && $target[$i]===$source[$j] && $target[$j]===$source[$i]))
      {
        $res++;
      }
    }
  }
  return $res;
}

// read the file
if(3 != count($argv))
{
  util::out('usage: process_sr input.csv output.csv');
  exit;
}

// we need the mapping from UID to true barcode from opal
$opalFile = 'OPAL_BL_MAP.csv';
$indata = read_csv($opalFile);
$opal_barcode_to_uid = array();
$opal_uid_to_barcode = array();
$opal_data = array();
//$site_keys = array();
foreach($indata as $data)
{
  $uid = $data['UID'];
  $opal_barcode_to_uid[$data['BARCODE']]=$uid;
  $opal_uid_to_barcode[$uid]=$data['BARCODE'];
  if($data['SITE']=='McMaster')$data['SITE']='Hamilton';
  $data['DATETIME']=
    DateTime::createFromFormat('Ymd H:i:s', $data['BEGIN']);
  $opal_data[$uid] = $data;
  //$site_keys[strtoupper(substr($data['SITE'],0,3))]='';
}
/*
$site_keys=array_keys($site_keys);
sort($site_keys);
var_dump($site_keys);
die();
*/
// INPUT raw sr cIMT data obtained with cimt_sr_pair_files.cxx on dicom SR data files
// data must contain UID, PATIENTID, STUDYDATE, STUDYTIME, SIDE, IMT_* measurements
$measure_keys=array(
  'IMT_Posterior_Average','IMT_Posterior_Max','IMT_Posterior_Min','IMT_Posterior_SD','IMT_Posterior_nMeas');

$do_cimt_reject = false;

$infile = $argv[1];
$outfile = $argv[2];
$indata = read_csv($infile);
$data = array('left'=>array(),'right'=>array());
$index = 1;
$num_row = count($indata);
$num_data = 0;
$num_reject_datetime = 0;
$num_invalid_cimt = 0;
$num_missing_barcode = 0;
$bogus_id = array();
$repair_id = array();
foreach($indata as $row)
{
  // group by UID, side, compute quality value based on codes, compute
  // length of code favoring longer (more verbose) codes
  util::show_status($index++,$num_row);
  $uid = $row['UID'];
  $side = strtolower($row['SIDE']);
  $laterality = strtolower($row['LATERALITY']);
  $multiple = $row['MULTIPLE'];
  $verify_side = $side == $laterality ? 1 : 0;
  $patid = $row['PATIENTID'];
  if(''==$patid || 'NA'==$patid)
  {
    util::out('WARNING: ' . $uid . ' assigning missing barcode [' . $side .']');
    $patid=$opal_uid_to_barcode[$uid];
    $num_missing_barcode++;
  }
  $stdate = $row['STUDYDATE'];
  if(''==$stdate || 'NA'==$stdate)
  {
    util::out('WARNING: ' . $uid . ' missing study date [' . $side .']');
    $num_reject_datetime++;
    continue;
  }
  $sttime = $row['STUDYTIME'];
  if(''==$sttime || 'NA'==$sttime)
  {
    util::out('WARNING: ' . $uid . ' missing study time [' . $side .']');
    $num_reject_datetime++;
    continue;
  }
  if(5 == strlen($sttime)) $sttime = '0' . $sttime;
  $site = $opal_data[$uid]['SITE'];
  if($site=='McMaster') $site='Hamilton';

  // is this a bogus id?
  $orig_id = $patid;
  if(!array_key_exists($patid,$opal_barcode_to_uid))
  {
    // can we fix this id and make it valid?
    $id = preg_replace('/[^0-9]/','',$patid);
    $opal_id = $opal_uid_to_barcode[$uid];
    if(''== $id || null == $id)
    {
      $patid = $opal_id;
      $repair_id[] = $res;
    }
    else
    {
      $opal_id_list = str_split($opal_id);
      $res = repair_barcode_id($id, $opal_id, $opal_id_list);
      if(false !== $res && array_key_exists($res,$opal_barcode_to_uid))
      {
        $patid = $res;
        $repair_id[] = $res;
      }
      else
      {
        $bogus_id[] = $patid;
      }
    }
  }

  $sr_data=
    array(
      'UID'=>$uid,
      'PATIENTID'=>$patid,
      'STUDYDATE'=>$stdate,
      'STUDYTIME'=>$sttime,
      'ORIGINALID'=>$orig_id,
      'BARCODE'=>$opal_uid_to_barcode[$uid],
      'SITE'=>$site,
      'SIDE'=>$side,
      'DATETIME'=>(DateTime::createFromFormat('Ymd Gis', $stdate . ' ' . $sttime)),
      'VALID'=>($patid == $opal_uid_to_barcode[$uid] && $site == $opal_data[$uid]['SITE']),
      'MATCH'=>($opal_uid_to_barcode[$uid]==$orig_id),
      'MULTIPLE'=>$multiple,
      'LATERALITY'=>$laterality,
      'VERIFYSIDE'=>$verify_side
      );

  $zero_count = 0;
  foreach($measure_keys as $key)
  {
    if('' == $row[$key] || 0==$row[$key]) $zero_count++;
    $sr_data[$key]=$row[$key];
  }
  if($do_cimt_reject &&
     $zero_count > 3)
  {
    $num_invalid_cimt++;
    continue;
  }

  // it is possible study datetime is not unique among sites, add a 3 char site key
  $site_key = strtoupper(substr($site,0,3));
  $dt_key = $stdate . $sttime . $site_key;
  $datetime_to_sr[$side][$dt_key][] = $sr_data;

  $num_data++;
}

$num_image_left = count($datetime_to_sr['left']);
$num_image_right = count($datetime_to_sr['right']);
$num_image_total = $num_image_left + $num_image_right;
util::out('found ' . $num_data . ' out of ' . $num_row . ' rows');
util::out('rejected datetimes ' . $num_reject_datetime . ' out of ' . $num_row );
if($do_cimt_reject)
{
  util::out('rejected cimt data ' . $num_invalid_cimt . ' out of ' . $num_row );
}
util::out('number of missing barcodes ' . $num_missing_barcode . ' out of ' . $num_row );
util::out('number of bogus barcodes ' . count(array_unique($bogus_id)));
util::out('number of repaired barcodes ' . count(array_unique($repair_id)));
util::out('found ' . $num_image_left . ' left images of ' . $num_image_total);
util::out('found ' . $num_image_right . ' right images of ' . $num_image_total);
util::out('estimated number of duplicate srs ' . ($num_data - $num_image_total));

// number of images that were exported under more than one participant id
$num_id_duplicate=0;
// get a sample of valid uid barcode image for each site
$valid_data=array();
$num_valid_sr = 0;
$num_valid_image = 0;
$num_indeterminate = 0;
$num_side_match=0;
foreach($datetime_to_sr as $side=>$side_data)
{
  foreach($side_data as $datetimesite_key=>$sr_data)
  {
    // check if this data is duplicate of opposite side
    $other_side = $side == 'left' ? 'right' : 'left';
    if(array_key_exists($datetimesite_key,$datetime_to_sr[$other_side]))
    {
      $sr_other = $datetime_to_sr[$other_side][$datetimesite_key];
      $n_other = count($sr_other);
      $n_this = count($sr_data);
      util::out('WARNING: sides match for key ' . $datetimesite_key . ' ' . 
        $n_other . ' ' . $n_this);
      if($n_other != $n_this || $n_this>1) die(); 
      $num_side_match++;
    }  

    $patientid_list=array();
    $uid_list=array();
    $direct_match_idx = false;
    foreach($sr_data as $idx_sr=>$data)
    {
      $patientid_list[$idx_sr]=$data['PATIENTID'];
      $uid_list[$idx_sr]=$data['UID'];
      if($data['MATCH'] && false == $direct_match_idx)
        $direct_match_idx = $idx_sr;
    }
    $patientid_unique = array_unique($patientid_list);
    $uid_unique = array_unique($uid_list);
    if(1 < count($patientid_unique) || 1 < count($uid_unique))
    {
      $num_id_duplicate++;
      $owner_idx = $direct_match_idx;

      // can we assign the correct id?
      // determine the true id uid owner of this image data
      // by comparing the dicom datetime to the opal estimated datetime
      // of the earliest event prior to acquisition
      if(false===$owner_idx)
      {
        $valid_time=array();
        foreach($patientid_list as $idx_sr => $patientid)
        {
          $data = $sr_data[$idx_sr];
          $valid_patientid = $data['VALID']; // the patient id matches the barcode and site for this uid
          $uid = $data['UID'];  // the uid

          // in order to compare to opal data, we need a valid uid / barcode pair
          // we assume that among the data elements, there is one among them
          // that the data belongs to in terms of time
          if($valid_patientid)
          {
            $dt1 = $data['DATETIME']; // the image date
            $dt2 = $opal_data[$uid]['DATETIME']; // the estimated time prior to the cIMT capture
            // get the time difference between image datetime and opal datetime
            $diff = strtotime($dt1->format('Y-m-d H:i:s')) - strtotime($dt2->format('Y-m-d H:i:s'));
            // a positive difference implies the acquisition occurred after the preceding interview events
            $valid_time[$idx_sr] = $diff;
            // the last valid id just in case there is only 1 valid time
            $owner_idx = $idx_sr;
          }
        }

        if(count($valid_time)>1 && asort($valid_time))
        {
          // if all times are negative then none of uid barcode pairs could rightfully claim ownership
          // determine the minimum positive time difference
          $owner_idx = false;
          foreach($valid_time as $idx_sr => $t)
          {
            if($t<0) continue;
            $owner_idx = $idx_sr;
            break;
          }
        }
      }
      if(false===$owner_idx)  // no valid uid id pair could be found in the data
      {
        util::out('indeterminate non-unique ' . util::flatten($patientid_list) . ' ' . util::flatten($uid_list) );
        // discard this data as orphaned
        $num_indeterminate++;
      }
      else
      {
        $data = $sr_data[$owner_idx];
        $uid = $data['UID'];
        if($data['PATIENTID']!=$data['BARCODE'])
        {
          util::out($uid. ': ' . $data['PATIENTID'] . ' <= ' . $data['BARCODE']);
          if($data['PATIENTID']!=$data['ORIGINALID'])
            $data['ORIGINALID']=$data['PATIENTID'];
          $data['PATIENTID'] = $data['BARCODE'];
        }
        $dt1 = $data['DATETIME']; // the image date
        $dt2 = $opal_data[$uid]['DATETIME'];
        util::out('assigning unique ownership ' .
          $uid . ' (' . $data['BARCODE'] . ') ' .
          $dt2->format('Y-m-d H:i:s') . ' > ' . $dt1->format('Y-m-d H:i:s') );

        $valid_data[$side][$datetimesite_key][] = $data;
        $num_valid_sr++;
        $num_valid_image++;
      }
    }
    else
    {
      // all of this data uses the same barcode and uid
      // how many are valid ?
      $owner_idx = $direct_match_idx;
      if(false===$owner_idx)
      {
        foreach($sr_data as $idx_sr=>$data)
        {
          if($data['VALID'])
          {
            $owner_idx = $idx_sr;
            break;
          }
        }
      }

      if(false===$owner_idx)
      {
        $max_match1 = -1;
        $idx1 = 0;
        $match1 = array();
        $max_match2 = -1;
        $idx2 = 0;
        $match2 = array();
        foreach($sr_data as $idx_sr=>$data)
        {
          $src = $data['BARCODE'];
          $trg = $data['PATIENTID'];
          $res = get_match_count($src,$trg);
          $match1[$idx_sr]=$res;
          if($res > $max_match1)
          {
            $max_match1 = $res;
            $idx1 = $idx_sr;
          }
          $src = array_unique(str_split($src));
          $trg = array_unique(str_split($trg));
          sort($src,SORT_STRING);
          sort($trg,SORT_STRING);
          $res = get_match_count($src,$trg);
          $match2[$idx_sr]=$res;
          if($res == count($src) && $res > $max_match2)
          {
            $max_match2 = $res;
            $idx2 = $idx_sr;
          }
        }
        if(!array_key_exists($sr_data[$idx1]['PATIENTID'], $opal_barcode_to_uid))
        {
          if($max_match1 > 6)
          {
            $owner_idx = $idx1;
            util::out('WARNING: accepting ' . count($sr_data) . ' imts for indeterminately owned image for ' .
              $sr_data[$idx1]['UID'] . ' match ' . $match1[$idx1] . ' ' . $sr_data[$idx1]['PATIENTID'] . ' ' . $sr_data[$idx1]['BARCODE']);
          }
          else if($max_match2 > 0)
          {
            $owner_idx = $idx2;
            util::out('WARNING: accepting ' . count($sr_data) . ' imts for indeterminately owned image for ' .
              $sr_data[$idx2]['UID'] . ' match ' . $match2[$idx2] . ' ' . $sr_data[$idx2]['PATIENTID'] . ' ' . $sr_data[$idx2]['BARCODE']);
          }
        }  
      }

      if(false!==$owner_idx)
      {
        $owner_data = $sr_data[$owner_idx];
        $num_valid_sr++;
        $num_valid_image++;
        if($owner_data['PATIENTID']!=$owner_data['BARCODE'])
        {
          util::out($uid. ': ' . $owner_data['PATIENTID'] . ' <= ' . $owner_data['BARCODE']);
          if($owner_data['PATIENTID']!=$owner_data['ORIGINALID'])
            $owner_data['ORIGINALID']=$owner_data['PATIENTID'];
          $owner_data['PATIENTID'] = $owner_data['BARCODE'];
        }
        $valid_data[$side][$datetimesite_key][] = $owner_data;
      }
      else
      {
        foreach($sr_data as $data)
        {
          util::out('WARNING: rejecting sr data for indeterminately owned '. $side. ' image for ' .
            util::flatten(array($data['UID'],$data['ORIGINALID'],$data['PATIENTID'],$data['BARCODE'])) . ' ' .
            (array_key_exists($data['PATIENTID'],$opal_barcode_to_uid)?' patient id already in use ' : ' patient id not in use'));
        }  
        $num_indeterminate += count($sr_data);
      }
    }
  }
}
util::out('number of duplicate image exports ' . $num_id_duplicate);
util::out('number of valid srs ' . $num_valid_sr);
util::out('number of valid images ' . $num_valid_image);
util::out('number of indeterminate ' . $num_indeterminate);
util::out('number of matched side images ' . $num_side_match);

// once we have gone through the sides separately
// look at UIDs with a left and a right

$header = array(
      'UID',
      'PATIENTID',
      'ORIGINALID',
      'STUDYDATE',
      'STUDYTIME',
      'SITE',
      'SIDE',
      'LATERALITY',
      'MULTIPLE',
  'IMT_Posterior_Average','IMT_Posterior_Max','IMT_Posterior_Min','IMT_Posterior_SD','IMT_Posterior_nMeas');

$file = fopen($outfile,'w');
if(false === $file)
{
  util::out('file ' . $outfile . ' cannot be opened');
  die();
}

fwrite($file, '"' . util::flatten($header,'","') . '"' . PHP_EOL);

$index = 1;
foreach($valid_data as $side=>$side_data)
{
  foreach($side_data as $datetimesite_key=>$data)
  {

    if(count($data)>1)
      foreach($data as $values)
      util::out('tie required ' . util::flatten(array_values($values)));
    $out_data = current($data);
    $str = '"' . util::flatten(array(
      $out_data['UID'],
      $out_data['PATIENTID'],
      $out_data['ORIGINALID'],
      $out_data['STUDYDATE'],
      $out_data['STUDYTIME'],
      $out_data['SITE'],
      $out_data['SIDE'],
      $out_data['LATERALITY'],
      $out_data['MULTIPLE'],
      $out_data['IMT_Posterior_Average'],
      $out_data['IMT_Posterior_Max'],
      $out_data['IMT_Posterior_Min'],
      $out_data['IMT_Posterior_SD'],
      $out_data['IMT_Posterior_nMeas']
      ),
      '","') . '"' . PHP_EOL;

    fwrite($file,$str);
    util::show_status($index++,$num_valid_image);
  }
}

fclose($file);

?>
