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

function get_quality($code_str)
{
  $q = 1;
  if('NONE'==$code_str)
    return $q;
  if(false!==strpos($code_str,'NO') ||
     false!==strpos($code_str,'NA') ||
     false!==strpos($code_str,'NI') )
  {
    $q = 3;
  }
  elseif(false!==strpos($code_str,'SB') ||
     false!==strpos($code_str,'ME') ||
     false!==strpos($code_str,'LO') )
  {
    $q = 2;
  }
  return $q;
}

// read the files
if(4 != count($argv))
{
  util::out('usage: process_ratings input_ratings.csv input_sr.csv output_ratings.csv');
  exit;
}

// we need the mapping from UID to true barcode from opal
$opalFile = 'OPAL_BL_MAP.csv';
$indata = read_csv($opalFile);
$opal_barcode_to_uid = array();
$opal_uid_to_barcode = array();
$opal_data = array();
$changed_sites = array();
$sites = array();
foreach($indata as $data)
{
  $uid = $data['UID'];
  $opal_barcode_to_uid[$data['BARCODE']]=$uid;
  $opal_uid_to_barcode[$uid]=$data['BARCODE'];
  $site = $data['SITE'];
  $site = trim(str_replace('University of', '', $site));
  $site = trim(str_replace('University', '', $site));
  if($site=='McMaster') $site='Hamilton';
  if($site != $data['SITE'])
  {
    $changed_sites[] = $site . ' <= ' . $data['SITE'];
    $data['SITE'] = $site;
  }
  $sites[]=$data['SITE'];

  $data['DATETIME']=
    DateTime::createFromFormat('Ymd H:i:s', $data['BEGIN']);
  $opal_data[$uid] = $data;
}
util::out('number of opal data elements ' . count($opal_data));
$changed_sites = array_unique($changed_sites);
util::out('changed sites: ' . util::flatten($changed_sites));
$sites = array_unique($sites);
util::out('sites: ' . util::flatten($sites));

// read in the sr data keys since these have a laterality attribute
$srFile = $argv[2];
$indata = read_csv($srFile);
$datetime_to_sr = array();
$num_sr = 0;
$uid_to_sr = array();
foreach($indata as $row)
{
  $side = strtolower($row['SIDE']);
  $stdate = $row['STUDYDATE'];
  $sttime = $row['STUDYTIME'];
  if(5 == strlen($sttime)) $sttime = '0' . $sttime;
  $site = $row['SITE'];
  $site_key = strtoupper(substr($site,0,3));
  $dt_key = $stdate . $sttime . $site_key;
  $sr_data = array(
    'UID'=>$row['UID'],
    'STUDYDATE'=>$stdate,
    'STUDYTIME'=>$sttime,
    'SITE'=>$site,
    'SIDE'=>$side,
    'LATERALITY'=>$row['LATERALITY'],
    'VERIFYSIDE'=>($side==$row['LATERALITY']),
    'PATIENTID'=>$row['PATIENTID'],
    'ORIGINALID'=>$row['ORIGINALID']
  );
  // sanity check
  if(array_key_exists($side,$datetime_to_sr) &&
     array_key_exists($dt_key,$datetime_to_sr[$side]))
  {
    util::out('ERROR: ' . $row['UID'] . ' ' . $side . ' sr datetime key already exists: '. $dt_key);
    die();
  }
  $datetime_to_sr[$side][$dt_key] = $sr_data;
  $uid_to_sr[$uid][$side] = $dt_key;
  $num_sr++;
}

// INPUT a raw csv Alder rating report for cIMT images
//
$wave = 'Baseline';
$expert = 'lisa';

$infile = $argv[1];
$outfile = $argv[3];
$indata = read_csv($infile);
$data = array('left'=>array(),'right'=>array());
$index = 1;
$num_row = count($indata);
$num_data = 0;
$num_reject_datetime = 0;
$num_missing_barcode = 0;
$bogus_id = array();
$repair_id = array();
$uid_to_rating = array();
$datetime_to_rating = array();
$num_found_sr = 0;
$num_unique = 0;
$num_incongruent_sr = 0;
foreach($indata as $row)
{
  // group by UID, side, compute quality value based on codes, compute
  // length of code favoring longer (more verbose) codes
  //util::show_status($index++,$num_row);
  // discard on wave
  // discard on derived=NA
  if($wave != $row['WAVE'] ||
     'NA' == $row['DERIVED'])
  {
    continue;
  }

  $uid = $row['UID'];
  $code = $row['CODE'];
  $rating = $row['DERIVED'];
  $rater = $row['RATER'];
  $wv = $row['WAVE'];
  $path = $row['PATH'];
  $visit = str_replace('-','',$row['VISITDATE']);
  $side = strtolower($row['SIDE']);
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
  $site = $row['SITE'];
  $site = trim(str_replace('University of', '', $site));
  $site = trim(str_replace('University', '', $site));
  if($site=='McMaster') $site='Hamilton';

  // sanity check
  if($site != $opal_data[$uid]['SITE'])
  {
    util::out('ERROR: ' . $uid . ' has incorrect site');
    die();
  }
  // it is possible study datetime is not unique among sites, add a 3 char site key
  $site_key = strtoupper(substr($site,0,3));
  $dt_key = $stdate . $sttime . $site_key;

  // try to link the side with the sr information
  $sr_key = null;

  // SR file for this rated still image
  if(array_key_exists($side,$datetime_to_sr) &&
     array_key_exists($dt_key,$datetime_to_sr[$side]))
  {
    $num_found_sr++;
    $sr_key = $dt_key;
  }
  else
    continue;

  // check if the sr_data is congruent
  if($datetime_to_sr[$side][$dt_key]['UID']!=$uid)
  {
    util::out('rejecting incongruent ' . $uid . ' < > ' . $datetime_to_sr[$side][$dt_key]['UID']);
    $num_incongruent_sr++;
    continue;
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

  $rater_data=
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
      'RATER'=>$rater,
      'RATING'=>$rating,
      'CODE'=>$code,
      'QUALITY'=>get_quality($code),
      'LENGTH'=>count(explode(',',trim($code))),
      'PATH'=>$path,
      'SR_KEY'=>$sr_key
      );

  $datetime_to_rating[$side][$dt_key][] = $rater_data;
  $num_data++;
  if(1==count($datetime_to_rating[$side][$dt_key]))
  {
    $num_unique++;
  }
}

util::out('number of sr: ' . $num_sr);
util::out('number of ratings with sr: ' . $num_found_sr);
util::out('number of ratings with incongruent sr: ' . $num_incongruent_sr);
util::out('number of unique rated image keys: ' . $num_unique);

// preprocess the keys ... make sure that left and right still images are not
// identical
//
$num_side_match=0;
$keys_unset['left'] = array();
$keys_unset['right'] = array();

// go through the data checking for ratings that have multiple UID's
// retain matching or valid data
//
$num_removed['left'] = 0;
$num_removed['right'] = 0;
$num_direct_match = 0;
$num_indirect_match = 0;
foreach($datetime_to_rating as $side=>$side_data)
{
  foreach($side_data as $datetimesite_key=>$rater_data)
  {
    $rater_uid = array();
    // is there a direct match?
    $num_in = count($rater_data);
    $idx_match = array_fill(0,$num_in,false);
    $idx_valid = $idx_match;
    foreach($rater_data as $idx=>$data)
    {
      $rater_uid[] = $data['UID'];
      if($data['MATCH']) $idx_match[$idx] = true;
      if($data['VALID']) $idx_valid[$idx] = true;
    }
    $data = array();
    $first = true;
    for($i = 0; $i < $num_in; $i++)
    {
      if($idx_match[$i])
      {
        if($first)
        {
          $first = false;
          $num_direct_match++;
        }
        $data[] = $rater_data[$i];
      }
    }
    if(0==count($data))
    {
      $first = true;
      for($i = 0; $i < $num_in; $i++)
      {
        if($idx_valid[$i])
        {
          if($first)
          {
            $first = false;
            $num_indirect_match++;
          }
          $data[] = $rater_data[$i];
        }
      }
    }

    $num_out = count($data);
    if(0==$num_out)
    {
      $keys_unset[$side][]=$datetimesite_key;
      $num_removed[$side] += $num_in;
    }
    else
    {
      $datetime_to_rating[$side][$datetimesite_key] = $data;
      $num_removed[$side] += $num_in - $num_out;

      // sanity check for multiple uids
      if(1<$num_out)
      {
        $uids = array();
        foreach($data as $value) $uids[] = $value['UID'];
        $uids = array_unique($uids);
        if(1 != count($uids))
        {
          util::out('WARNING: multiple uids share the same image: ' . util::flatten($uids));
          die();
        }
      }
    }
  }
}

foreach($keys_unset as $side => $key_values)
{
  $key_values = array_unique($key_values);
  foreach($key_values as $key)
  {
    unset($datetime_to_rating[$side][$key]);
  }
}
util::out('number of images with direct id match: '  . $num_direct_match);
util::out('number of images with indirect (valid) id match: '  . $num_indirect_match);
util::out('number of left ratings removed: '  . $num_removed['left']);
util::out('number of right ratings removed: '  . $num_removed['right']);

// remove duplicates
$keys_unset['left'] = array();
$keys_unset['right'] = array();
$num_removed['left'] = 0;
$num_removed['right'] = 0;
foreach($datetime_to_rating as $side=>$side_data)
{
  foreach($side_data as $datetimesite_key=>$rater_data)
  {
    // check if this data is a duplicate of the opposite side
    // retain the data that has the expected side
    //
    $this_sr = array();
    foreach($rater_data as $data)
    {
      if(null!==$data['SR_KEY']) $this_sr[]=$data['SR_KEY'];
      // sanity check
      if($data['SIDE']!=$side)
      {
        util::out('ERROR: ' . $side . ' side consistency error');
        die();
      }
    }
    $num_this_sr = count($this_sr);
    $num_this = count($rater_data);
    // sanity check
    if(0 < $num_this_sr)
    {
      if($num_this_sr != $num_this)
      {
        util::out('ERROR: missing expected sr keys');
        die();
      }
      $this_sr = current(array_unique($this_sr));
      $this_sr = $datetime_to_sr[$side][$datetimesite_key];
    }
    else
    {
      // sanity check
      util::out('ERROR: no sr attributed to rating data');
      die();
    }

    $other_side = $side == 'left' ? 'right' : 'left';
    if(array_key_exists($datetimesite_key,$datetime_to_rating[$other_side]))
    {
      $rater_other_data = $datetime_to_rating[$other_side][$datetimesite_key];
      $that_sr = array();
      foreach($rater_other_data as $data)
      {
        if(null!==$data['SR_KEY']) $that_sr[]=$data['SR_KEY'];
        // sanity check
        if($data['SIDE']!=$other_side)
        {
          util::out('ERROR: ' . $other_side . ' side consistency error');
          die();
        }
      }
      $num_that_sr = count($that_sr);
      $num_that = count($rater_other_data);
      // sanity check
      if(0 < $num_that_sr)
      {
        if($num_that_sr != $num_that)
        {
          util::out('ERROR: missing expected sr keys');
          die();
        }
        $that_sr = current(array_unique($that_sr));
        $that_sr = $datetime_to_sr[$other_side][$datetimesite_key];
      }
      else
      {
        // sanity check
        util::out('ERROR: no sr attributed to rating data');
        die();
      }

      // which one to keep?
      $keep = array();
      if(null!==$this_sr)
      {
        $data = current($rater_data);
        if($this_sr['SIDE']==$data['SIDE'])
          $keep[] = $side;
      }
      if(null!==$that_sr)
      {
        $data = current($rater_other_data);
        if($that_sr['SIDE']==$data['SIDE'])
          $keep[] = $side;
      }
      $keep = array_unique($keep);
      // sanity check
      if(1 < count($keep))
      {
        util::out('ERROR: cant discern sides');
        die();
      }
      else if(1 == count($keep) && $keep[0] == $side)
      {
        // we reject the other side data
        if(!array_key_exists($datetimesite_key, $keys_unset[$other_side]))
        {
          $keys_unset[$other_side][] = $datetimesite_key;
          $num_removed[$other_side] += count($rater_other_data);
        }
      }

      $num_side_match++;
    }
  }
}

util::out('number of side matches: ' . $num_side_match);
util::out('number of left side ratings removed: ' . $num_removed['left']);
util::out('number of right side ratings removed: ' . $num_removed['right']);

// remove the invalid data
if(0 < $num_side_match)
{
  foreach($keys_unset as $side => $key_values)
  {
    $key_values = array_unique($key_values);
    foreach($key_values as $key)
    {
      unset($datetime_to_rating[$side][$key]);
    }
  }
}

$header = array(
  'UID','SIDE','RATER','CODE','RATING','QUALITY',
  'PATIENTID','ORIGINALID','SITE','STUDYDATE','STUDYTIME');
$validation=array();
$file = fopen($outfile,'w');
if(false === $file)
{
  util::out('file ' . $outfile . ' cannot be opened');
  die();
}

fwrite($file, '"' . util::flatten($header,'","') . '"' . PHP_EOL);
$index = 1;
$num_valid_rating = 0;
foreach($datetime_to_rating as $side=>$side_data)
{
  foreach($side_data as $datetimesite_key=>$data)
  {
    $num_valid_rating += count($data);
  }
}
foreach($datetime_to_rating as $side=>$side_data)
{
  foreach($side_data as $datetimesite_key=>$data)
  {
    $out_data = null;
    if(count($data)>1)
    {
      $rater_data = array();
      foreach($data as $values)
      {
        $rater_data[$values['RATER']] = $values;
      }
      // prefer the expert's rating
      if(array_key_exists($expert,$rater_data))
      {
        $out_data = $rater_data[$expert];
      }
      else  // tie breaking
      {
        $unique = array();
        foreach($rater_data as $rater_key=>$values)
        {
          $unique[$values['CODE']] = $rater_key;
        }
        if(1 == count($unique))
        {
          $rater = $unique[current(array_keys($unique))];
          $out_data = $rater_data[$rater];
        }
        else
        {
          $unique = array();
          foreach($rater_data as $rater_key=>$values)
          {
            $unique[$values['QUALITY']][$values['LENGTH']] = $rater_key;
          }
          $qmin = min(array_keys($unique));  // pick the best quality
          $lmax = max(array_keys($unique[$qmin]));  // pick the most verbose

          $rater = $unique[$qmin][$lmax];
          $out_data = $rater_data[$rater];
        }
      }
    }
    else
    {
      $out_data = current($data);
    }

    $str = '"' . util::flatten(array(
      $out_data['UID'],
      $out_data['SIDE'],
      $out_data['RATER'],
      $out_data['CODE'],
      $out_data['RATING'],
      $out_data['QUALITY'],
      $out_data['PATIENTID'],
      $out_data['ORIGINALID'],
      $out_data['SITE'],
      $out_data['STUDYDATE'],
      $out_data['STUDYTIME']),
      '","') . '"' . PHP_EOL;

    fwrite($file,$str);
    util::show_status($index++,$num_valid_rating);
  }
}

fclose($file);

?>
