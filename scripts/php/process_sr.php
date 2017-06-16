<?php

require_once('/home/dean/files/repository/php_util/util.class.php');
util::initialize();

// count the number of sequential matches including possible single swaps
// source and target must be the same size
//
function get_match_count($source,$target)
{
  $res = 0;
  if(!is_array($source))
    $source = str_split($source);
  if(!is_array($target))
    $target = str_split($target);
  $n = count($source);
  if($n == count($target))
  {
    for($i = 0; $i < $n; $i++)
    {
      $j = $i+1;
      if($source[$i] == $target[$i] ||
         ($j < $n  && $target[$i] == $source[$j] && $target[$j] == $source[$i]))
      {
        $res++;
      }
    }
  }
  return $res;
}

// given an unmatching barcode, try to generate a valid substitution
//
function repair_barcode_id($id, $opal_id, $opal_id_list)
{
  if($id == $opal_id) return $id;
  $id_list = str_split($id);
  $id_length = count($id_list);
  $res = implode(array_intersect($opal_id_list,$id_list));
  $ndiff = count(array_diff($id_list, $opal_id_list));
  if($res == $opal_id && 8 != $id_length)
  {
    return $res;
  }
  if(7 == $id_length && 0 == $ndiff)
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

  if(8 == $id_length)
  {
    $res = get_match_count($opal_id_list, $id_list);
    if(6 < $res)
    {
      util::out('repairing by direct match count: ' . $id . ' <= ' . $opal_id);
      return $opal_id;
    }
    // do second match attempt for cases when only 1 digit difference
    $src = $opal_id_list;
    $trg = $id_list;
    sort($src,SORT_STRING);
    sort($trg,SORT_STRING);
    $res = get_match_count($src,$trg);
    if(6 < $res)
    {
      util::out('repairing by sorted match count: ' . $id . ' <= ' . $opal_id);
      return $opal_id;
    }
    util::out('failed to repair by match count: ' . $id . ' <= ' . $opal_id);
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
}
util::out('number of opal data elements ' . count($opal_data));
/*
$site_keys=array_keys($site_keys);
sort($site_keys);
var_dump($site_keys);
die();
*/
// INPUT raw sr cIMT data obtained with cimt_sr_pair_files_dcmtk.cxx on dicom SR data files
// data must contain UID, PATIENTID, STUDYDATE, STUDYTIME, SIDE, LATERALITY, IMT_* measurements, VERIFYSIDE, MULTIPLE
$measure_keys=array(
  'IMT_Posterior_Average','IMT_Posterior_Max','IMT_Posterior_Min','IMT_Posterior_SD','IMT_Posterior_nMeas');

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
$uid_to_sr = array();
foreach($indata as $row)
{
  // group by UID, side, compute quality value based on codes, compute
  // length of code favoring longer (more verbose) codes
  //util::show_status($index++,$num_row);
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

  // sanity check
  if($site != $opal_data[$uid]['SITE'])
  {
    util::out('ERROR: ' . $uid . ' has incorrect site');
    die();
  }

  $orig_id = $patid;
  $opal_id = $opal_uid_to_barcode[$uid];
  // is this a bogus id?
  if($orig_id != $opal_id)
  {
    $opal_id_list = str_split($opal_id);
    // if the id is not in use already
    $do_repair = false;
    if(!array_key_exists($patid,$opal_barcode_to_uid))
    {
      $do_repair = true;
    }
    else
    {
      // it is in use, see if the id is already owned by the site
      // get the mapped uid from the id
      $mapped_uid = $opal_barcode_to_uid[$patid];
      $mapped_site = $opal_data[$mapped_uid]['SITE'];
      if($mapped_site != $site)
      {
        util::out('correction can be made ' . $patid . ' ' . $site . ' < > ' . $mapped_site);
        $do_repair = true;
      }
    }

    if($do_repair)
    {
      // can we fix this id and make it valid?
      $id = preg_replace('/[^0-9]/','',$patid);
      if(''== $id || null == $id)
      {
        $patid = $opal_id;
        $repair_id[] = $res;
      }
      else
      {
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
  }

  $sr_data=
    array(
      'UID'=>$uid,
      'PATIENTID'=>$patid,    // repaired or original id from dicom PatientId tag
      'STUDYDATE'=>$stdate,   // dicom StudyDate tag
      'STUDYTIME'=>$sttime,   // dicom StudyTime tag
      'ORIGINALID'=>$orig_id, // original id from dicom PatientId tag
      'BARCODE'=>$opal_id,    // expected id from Opal
      'SITE'=>$site,          // expected site from Opal
      'SIDE'=>$side,          // expected side from Opal
      'DATETIME'=>(DateTime::createFromFormat('Ymd Gis', $stdate . ' ' . $sttime)),
      'VALID'=>($patid == $opal_id && $orig_id != $opal_id),
      'MATCH'=>($orig_id == $opal_id),
      'MULTIPLE'=>$multiple,     // sr file contained multiple sets of measurements
      'LATERALITY'=>$laterality, // sr Laterality
      'VERIFYSIDE'=>$verify_side // sr Laterality matches expected side
      );

  // reject sr data that has no value
  $zero_count = 0;
  foreach($measure_keys as $key)
  {
    if('' == $row[$key] || 0 == $row[$key]) $zero_count++;
    $sr_data[$key] = $row[$key];
  }
  if(5 == $zero_count)
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

// preprocess the keys ... make sure that left and right SR files are not
// identical
//
$num_side_match=0;
$keys_unset['left'] = array();
$keys_unset['right'] = array();
foreach($datetime_to_sr as $side=>$side_data)
{
  foreach($side_data as $datetimesite_key=>$sr_data)
  {
    // check if this data is a duplicate of the opposite side
    // retain the data that has the expected side: Opal side matches sr Laterality dicom tag
    //
    $other_side = $side == 'left' ? 'right' : 'left';
    if(array_key_exists($datetimesite_key,$datetime_to_sr[$other_side]))
    {
      $sr_other = $datetime_to_sr[$other_side][$datetimesite_key];
      $n_other = count($sr_other);
      $n_this = count($sr_data);
      util::out('WARNING: sides match for key ' . $datetimesite_key);
      if(1 != $n_other && 1 != $n_this)
      {
        util::out('ERROR: failed on mismatch process checking 1');
        die();
      }
      $num_side_match++;
      $sr_other = current($sr_other);
      $sr_this = current($sr_data);
      // sanity check that the data are indeed the same
      //
      if($sr_this['IMT_Posterior_Average'] != $sr_other['IMT_Posterior_Average'] ||
         $sr_this['VERIFYSIDE'] == $sr_other['VERIFYSIDE'])
      {
        util::out('ERROR: failed on mismatch process checking 2');
        die();
      }
      // mark the incorrect side data for removal
      if(0 == $sr_this['VERIFYSIDE'])
        $keys_unset[$side][] = $datetimesite_key;
      else
        $keys_unset[$other_side][] = $datetimesite_key;
    }
  }
}

// remove the invalid sr data
if(0 < $num_side_match)
{
  foreach($keys_unset as $side => $key_values)
  {
    $key_values = array_unique($key_values);
    foreach($key_values as $key)
    {
      unset($datetime_to_sr[$side][$key]);
      $num_data--;
    }
  }
}
$num_side_match/=2;

// create mapping from uid and side to sr key
// sr keys are unique per side per uid
// since the Opal view is scripted to only release
// one (the last exported) sr file per side
// see cIMT_SR_BL.xml
//
foreach($datetime_to_sr as $side=>$side_data)
{
  foreach($side_data as $datetimesite_key=>$sr_data)
  {
    foreach($sr_data as $data)
    {
      $uid_to_sr[$data['UID']][$data['SIDE']][] = $datetimesite_key;
    }
  }
}

// count the number of uid's with single, bilateral or swapped sr data
// swapping sides when required
//
$num_single_side = 0;
$num_both_side = 0;
$num_swap_side = 0;
$num_sr['left'] = 0;
$num_sr['right'] = 0;
$num_dup_sr['left'] = 0;
$num_dup_sr['right'] = 0;
foreach($uid_to_sr as $uid => $side_data)
{
  if(1 == count($side_data))
  {
    $num_single_side++;
  }
  else
  {
    // check if the left and right are mismatched or shared among other uid's
    $left_key = $side_data['left'];
    $right_key = $side_data['right'];
    // sanity check
    if(1 != count($left_key) || 1 != count($right_key))
    {
      util::out('ERROR: failed on swap checking 1');
      die();
    }
    $left_key = current($left_key);
    $right_key = current($right_key);
    if(array_key_exists($left_key, $datetime_to_sr['left']) &&
       array_key_exists($right_key, $datetime_to_sr['right']))
    {
      $num_both_side++;
      $left_sr_data = $datetime_to_sr['left'][$left_key];
      $right_sr_data = $datetime_to_sr['right'][$right_key];
      // get the data for the current uid
      $uid_left = null;
      $idx_left = -1;
      foreach($left_sr_data as $idx => $data)
      {
        if($uid == $data['UID'])
        {
          $uid_left[] = $data;
          $idx_left = $idx;
        }
      }
      $uid_right = null;
      $idx_right = -1;
      foreach($right_sr_data as $idx => $data)
      {
        if($uid == $data['UID'])
        {
          $idx_right = $idx;
          $uid_right[] = $data;
        }
      }
      if(!is_null($uid_left) && !is_null($uid_right) &&
         1 == count($uid_left) && 1 == count($uid_right))
      {
        $left_sr = current($uid_left);
        $right_sr = current($uid_right);
        if(0 == $left_sr['VERIFYSIDE'] && 0 == $right_sr['VERIFYSIDE'])
        {
          $left_sr['VERIFYSIDE'] = 1;
          $right_sr['VERIFYSIDE'] = 1;
          $tmp = $left_sr;
          $left_sr = $right_sr;
          $right_sr = $tmp;
          $datetime_to_sr['right'][$right_key][$idx_right] = $right_sr;
          $datetime_to_sr['left'][$left_key][$idx_left] = $left_sr;
          $num_swap_side++;
        }
      }
      else
      {
        // sanity check
        util::out('ERROR: failed on swap checking 2');
        die();
      }
    }
    else
    {
      // sanity check
      util::out('ERROR: failed on swap checking 3');
      die();
    }
  }
  foreach($side_data as $side=>$data)
    $num_sr[$side]++;
}

// reset the uid to side to sr key mapping
if(0 < $num_swap_side)
{
  $uid_to_sr = array();
  foreach($datetime_to_sr as $side=>$side_data)
  {
    foreach($side_data as $datetimesite_key=>$sr_data)
    {
      foreach($sr_data as $data)
      {
        $uid_to_sr[$data['UID']][$data['SIDE']][] = $datetimesite_key;
      }
    }
  }
}

$num_keys_left = count($datetime_to_sr['left']);
$num_keys_right = count($datetime_to_sr['right']);
$num_sr_total = $num_sr['left'] + $num_sr['right'];
util::out('found ' . $num_data . ' out of ' . $num_row . ' rows');
util::out('rejected datetimes ' . $num_reject_datetime . ' out of ' . $num_row );
util::out('number of rejected zero-filled sr data ' . $num_invalid_cimt . ' out of ' . $num_row );
util::out('number of missing sr patient ids ' . $num_missing_barcode . ' out of ' . $num_row );
util::out('number of bogus sr patient ids  ' . count(array_unique($bogus_id)));
util::out('number of repaired sr patient ids ' . count(array_unique($repair_id)));
util::out('found ' . $num_sr['left'] . ' left srs of ' . $num_sr_total . ' among ' . $num_keys_left . ' keys');
util::out('found ' . $num_sr['right'] . ' right srs of ' . $num_sr_total . ' among ' . $num_keys_right . ' keys');
util::out('number of uids with single side ' . $num_single_side);
util::out('number of uids with both sides ' . $num_both_side);
util::out('number of uids with swapped sides ' . $num_swap_side);
util::out('number of matched sides ' . $num_side_match);

// sanity check
foreach($uid_to_sr as $uid => $side_data)
{
  foreach($side_data as $side => $datetimesite_key)
  {
    if(1<count($datetimesite_key))
    {
      util::out('ERROR: ' . $uid . ' has more than one sr attributed to ' . $side . ' side');
      die();
    }
    else
      $uid_to_sr[$uid][$side] = current($datetimesite_key);
  }
}

// go through the sr keys
// for each sr key with multiple data
// determine if there is a single direct match among the set of data
// if there is, unset the incorrect elements
util::out('final processing stage ... ');
$valid_data = array();
$num_valid_sr = 0;
$keys_unset['left'] = array();
$keys_unset['right'] = array();
$keys_uid_unset = array();
$num_orphan_sr['left'] = 0;
$num_orphan_sr['right'] = 0;
foreach($datetime_to_sr as $side=>$side_data)
{
  foreach($side_data as $datetimesite_key=>$sr_data)
  {
    $num_current = count($sr_data);
    if(1<$num_current) $num_dup_sr[$side]++;
    $match_idx = array();
    $valid_idx = array();
    foreach($sr_data as $idx=>$data)
    {
      if($data['MATCH']) $match_idx[] = $idx;
      if($data['VALID']) $valid_idx[] = $idx;
    }
    // we have an exact match to Opal barcode and sr dicom patient id tag
    $num_match = count($match_idx);
    $num_valid = count($valid_idx);

    $idx_valid = -1;
    if(1 == $num_match)
    {
      // case 1: num_match == 1 (trivial case)
      $idx_valid = current($match_idx);
    }
    else if(1 < $num_match)
    {
      // multiple participants share the same sr file
      // sort by opal datetime difference to the file datetime
      // select the one closest to the opal datetime
      // a positive difference implies the acquisition occurred after the preceding interview events
      //
      $valid_time = array();
      foreach($match_idx as $idx)
      {
        $uid = $sr_data[$idx]['UID'];
        $opal_dt = $opal_data[$uid]['DATETIME'];
        $sr_dt = $sr_data[$idx]['DATETIME'];
        $valid_time[$idx] =
          strtotime($sr_dt->format('Y-m-d H:i:s')) - strtotime($opal_dt->format('Y-m-d H:i:s'));
      }
      // get the minimum positive time difference in sec
      $min_diff = 1000000000;
      $idx_min = -1;
      foreach($valid_time as $idx => $diff)
      {
        if($diff > 0 && $min_diff > $diff)
        {
          $min_diff = $diff;
          $idx_min = $idx;
        }
      }
      if(-1 != $idx_min)
      {
        $idx_valid = $idx_min;
        util::out('WARNING: ' . $num_match .
          ' direct matches, selecting based on time difference of ' . gmdate('H:i:s',$valid_time[$idx_min]));
      }
      else
      {
        util::out('WARNING: ' . $num_match .
          ' direct matches, cannot determine valid time difference, rejecting');
      }
    }
    else // no direct matches
    {
      // case 2: corrected patient id tag identifier
      if(1 < $num_valid)
      {
        util::out('WARNING: ' . $num_valid . ' indirect matches');
      }
      else if(1 == $num_valid)
      {
        $idx_valid = current($valid_idx);
      }
      else
      {
        // no valid id can be attributed to this sr key data pair
        if(1 == $num_current)
        {
          $data = current($sr_data);
          $uid = $data['UID'];
          $patid = $data['ORIGINALID'];
          // is the original incorrect id a valid id, if it is, which uid and site owns it?
          if(array_key_exists($patid,$opal_barcode_to_uid))
          {
            // which site does it belong to?
            $mapped_uid = $opal_barcode_to_uid[$patid];
            $mapped_site = $opal_data[$mapped_uid]['SITE'];
            // is the original incorrect id belonging to this site?
            if($mapped_site != $site)
            {
              // if this incorrect id is in use but belongs to another site
              // then by virtue of the export process, it must belong to the current id
              // we can correct the id in this case and use the data

              //util::out('accepting ' . $uid . ' valid format: ' . $data['ORIGINALID'] . ' <= ' . $data['BARCODE'] );
              $sr_data[0]['PATIENTID'] = $sr_data[0]['BARCODE'];
              $idx_valid = 0;
            }
            else
            {
              // it could be that this participant received another participant's data
              // based on the patient id in the sr file, get the uid, side and the key for their data
              if(array_key_exists($mapped_uid,$uid_to_sr) &&
                 array_key_exists($side,$uid_to_sr[$mapped_uid]))
              {
                $other_key = $uid_to_sr[$mapped_uid][$side];
                util::out('----found other key for ' . $side . ' ' . $mapped_uid .
                   ': ' . $other_key . ' <= ' . $datetimesite_key);

                // get the data for that key
                $other_data = $datetime_to_sr[$side][$other_key];
                // has that key been validated yet?
                if(array_key_exists($side,$valid_data) &&
                   array_key_exists($other_key,$valid_data[$side]))
                {
                  util::out(' this data already accepted to ' . $mapped_uid);
                  $keys_unset[$side][] = $datetimesite_key;
                }
                else
                {
                  util::out(' this data pending acceptance to ' . $mapped_uid);
                }
              }
              else
              {
                // if we get here, then we have a patient id tag that doesnt belong to the
                // attached Opal participant and we have no way to correct for it
                // the Opal participant that it could belong to doesnt have data either
                // but it is highly unlikely that their data was never exported to Onyx from the vivid-i source
                util::out('------other uid has no data');
                $num_orphan_sr[$side]++;
              }
            }
          }
          else
          {
            // this is an unknown id that couldnt be repaired
            //
            //util::out('accepting ' . $uid . ' invalid unknown: ' . $data['ORIGINALID'] . ' <= ' . $data['BARCODE'] );
            $sr_data[0]['PATIENTID'] = $sr_data[0]['BARCODE'];
            $idx_valid = 0;
          }
        }
        else
        {
          // sanity check
          util::out('ERROR: ' . $num_current . ' invalid data sets attached to sr key');
          die();
        }
      }
    }

    if(-1 != $idx_valid)
    {
      $valid_data[$side][$datetimesite_key] = $sr_data[$idx_valid];
      $keys_unset[$side][] = $datetimesite_key;
      $keys_uid_unset[$side][] = $sr_data[$idx_valid]['UID'];
    }
    else
    {
      $num_orphan_sr[$side]++;
    }
  }
}

util::out('number of shared left srs: ' . $num_dup_sr['left']);
util::out('number of shared right srs: ' . $num_dup_sr['right']);
util::out('number of orphaned left srs: ' . $num_orphan_sr['left']);
util::out('number of orphaned right srs: ' . $num_orphan_sr['right']);

$num_chk=0;
foreach($uid_to_sr as $uid=>$side_data)
{
  foreach($side_data as $side=>$key) $num_chk++;
}
util::out('uids attached to srs: '. $num_chk);

if(0<count($valid_data))
{
  foreach($keys_unset as $side => $key_values)
  {
    $key_values = array_unique($key_values);
    foreach($key_values as $key)
    {
      unset($datetime_to_sr[$side][$key]);
      $num_valid_sr++;
    }
  }
  foreach($keys_uid_unset as $side => $key_values)
  {
    foreach($key_values as $key)
      unset($uid_to_sr[$key][$side]);
  }
  util::out('found ' . $num_valid_sr . ' valid srs in ' . $num_sr_total);
}

// how many UID's are still in play per side?
$num_chk=0;
foreach($uid_to_sr as $uid=>$side_data)
{
  foreach($side_data as $side=>$key) $num_chk++;
}
util::out('remaining uids attached to srs: '. $num_chk);

// how many srs are still inplay per size
$num_chk=0;
$num_uid_attached = array();
foreach($datetime_to_sr as $side=>$side_data)
{
  foreach($side_data as $datetimesite_key=>$sr_data)
  {
    $num_chk++;
    foreach($sr_data as $data)
      $num_uid_attached[] = $data['UID'];
  }
}
$num_uid_attached = count(array_unique($num_uid_attached));
util::out($num_chk . ' remaining srs attached to ' . $num_uid_attached . ' uids');

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
    $out_data = $data;
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
    util::show_status($index++,$num_valid_sr);
  }
}

fclose($file);

?>
