<?php

require_once('/home/dean/files/sandbox/php/util.class.php');
util::initialize();

// data keys to use for asserting uniqueness
//
$data_keys = array(
  'sr' =>
    array(
      'STUDYDATE',
      'STUDYTIME',
      'IMT_Posterior_Average',
      'IMT_Posterior_Max',
      'IMT_Posterior_Min',
      'IMT_Posterior_SD',
      'IMT_Posterior_nMeas'),
  'rating' =>
    array(
      'STUDYDATE',
      'STUDYTIME',
      'RATER',
      'CODE',
      'RATING',
      'QUALITY')
);

// read and parse a csv file
//
function read_csv($fileName, $uid_shift = true)
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
      continue;
    }
    $line1 = explode('","',trim($line,"\n\"\t"));

    if(count($header)!=count($line1))
    {
      util::out($fileName);
      util::out(count($header) . ' < > ' . count($line1));
      util::out('Error: line (' . $line_count . ') wrong number of elements ' . $line);
      die();
    }
    $line1 = array_combine($header,$line1);
    if($uid_shift)
    {
      $uid = $line1['UID'];
      array_shift($line1);
      $data[$uid] = $line1;
    }
    else
      $data[] = $line1;
  }
  fclose($file);
  return $data;
}

// returns the siblings of this data element that match on its data keys
// or false if the data is actually unique
//
function unique_test($uid, $data_list, $data_map, $data_key_list)
{
  // the data for this uid
  $data = $data_list[$uid];
  $site = $data['SITE'];
  $date = $data['DATE'];
  $result = true;

  if(array_key_exists($date,$data_map))
  {
    // get the siblings uids for this date
    $siblings = $data_map[$date];
    // get the data for this data list
    $data_str = array();

    foreach($data_key_list as $data_key)
    {
      $data_str[] = $data[$data_key];
    }
    $data_str = util::flatten($data_str,',');
    foreach($siblings as $key=>$id)
    {
      if($uid==$key) continue;
      $compare_data = $data_list[$key];
      if($site!=$compare_data['SITE']) continue;
      $compare_str = array();
      foreach($data_key_list as $data_key)
      {
        $compare_str[] = $compare_data[$data_key];
      }
      if(util::flatten($compare_str,',')==$data_str)
      {
        $result[$key]=$id;// add the uid id key value pair as a match
      }
    }
  }
  return $result;
}

function compare_data($source_uid, $target_uid, $data_list, $data_key_list, &$res)
{
  $source_str = array();
  $target_str = array();
  $has_src = array_key_exists($source_uid,$data_list);
  $has_trg = array_key_exists($target_uid,$data_list);
  foreach($data_key_list as $data_key)
  {
    $source_str[] = $has_src ? $data_list[$source_uid][$data_key] : 'NA';
    $target_str[] = $has_trg ? $data_list[$target_uid][$data_key] : 'NA';
  }
  $source_str = util::flatten($source_str,',');
  $target_str = util::flatten($target_str,',');
  $match = $source_str == $target_str;
  $glue = $match ? ' == ' : ' < > ';
  $res = $source_str . $glue . $target_str;
  return $match;
}

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


// read the files

//UID, PATIENTID, STUDYDATE, STUDYTIME IMT measurements dicom data from SR files
// SR data should match to Alder still image and rating data
// Data are already provided with non-empty original, repaired or assigned barcodes
// Data with invalid cIMT measurements (zero valued) are already prunded out using process_sr.php
// No data elements can have empty or NA
$srFile = 'test_sr_BL.csv';

// UID, SITE, BARCODE, BEGIN from Opal : defacto gold standard ID information
// No data elements can have empty or NA
$opalFile = 'OPAL_BL_MAP.csv';

// UID, SIDE, RATER, CODES, RATING, QUALITY, PATIENTID, SITE, STUDYDATE, STUDYTIME
// the ratings obtained after running process_ratings.php on raw Alder rating data
// the 3 level quality index: good, re-analyzable, fail
// repeat ratings of the same still image are reduced to one optimal choice
// Data are already provided with non-empty original, repaired or assigned barcodes
// No data elements can have empty or NA
//
$ratingFile = 'test_rating_BL.csv';

// create date and datetime objects and correct for McMaster alias for Hamilton site name
//
$opal = read_csv($opalFile);
$site_names = array();
foreach($opal as $uid=>$data)
{
  $opal[$uid]['DATE']='NA';
  if($opal[$uid]['SITE']=='McMaster') $opal[$uid]['SITE']='Hamilton';
  $site_names[]=$opal[$uid]['SITE'];
  if('NA'!=$data['BEGIN'])
  {
    $dt = DateTime::createFromFormat('Ymd G:i:s', $data['BEGIN']);
    $date = $dt->format('Ymd');
    $opal[$uid]['DATE']=$date;
    $opal[$uid]['DATETIME']=$dt;
  }
}
$num_uid = count($opal);
$site_names = array_unique($site_names);
// create barcode to uid mappings
//
$barcode_to_uid = array();
$uid_to_barcode = array();
foreach($opal as $uid=>$opal_data)
{
  $opal_id = $opal_data['BARCODE']; // the true visit barcode
  if(array_key_exists($opal_id,$barcode_to_uid))
    util::out('WARNING: non-unique opal barcode mapping ' .
      $opal_id .  ': ' . $uid . ' <= ' . $barcode_to_uid[$opal_id]);
  else
    $barcode_to_uid[$opal_id]=$uid;
  $uid_to_barcode[$uid] = $opal_id;  
}

$var_side = array('R'=>'RIGHT','L'=>'LEFT');

// parse Alder rating data into left and right data sets
//
$ratings = read_csv($ratingFile, false);
$rating_L = array();
$rating_R = array();
foreach($ratings as $rating_data)
{
  $key = substr(strtoupper($rating_data['SIDE']),0,1);
  $uid = $rating_data['UID'];
  if(array_key_exists($uid,${'rating_' . $key})) util::out('WARNING: non-unique UID ' . $uid);
  ${'rating_' . $key}[$uid] = $rating_data;
}

// parse SR data into left and right data sets
//
$srs = read_csv($srFile, false);
$sr_L=array();
$sr_R=array();
foreach($srs as $sr_data)
{
  $key = substr(strtoupper($sr_data['SIDE']),0,1);
  $uid = $sr_data['UID'];
  if(array_key_exists($uid,${'sr_' . $key})) util::out('WARNING: non-unique UID ' . $uid);
  ${'sr_' . $key}[$uid] = $sr_data;
}

$set = array('rating','sr');

// mapping from id to 1 or more UID's
$sr_L_barcodemap=array();
$sr_R_barcodemap=array();
$rating_L_barcodemap=array();
$rating_R_barcodemap=array();

// mapping from content date to 1 or more barcode id's
$sr_L_map=array();
$sr_R_map=array();
$rating_L_map=array();
$rating_R_map=array();

$num_rating_total=array('LEFT'=>0,'RIGHT'=>0);
$num_sr_total=array('LEFT'=>0,'RIGHT'=>0);
$index = 1;
foreach($set as $var)
{
  foreach($var_side as $key=>$side)
  {
    util::show_status($index++,4);
    $var_name = $var .  '_' . $key;
    $var_map = $var_name . '_map';
    $var_barcodemap = $var_name . '_barcodemap';
    foreach(${$var_name} as $uid=>$uid_data)
    {
      ${'num_'.$var.'_total'}[$side]++;

      // get the opal id
      $opal_id = $opal[$uid]['BARCODE'];
      $id = $uid_data['PATIENTID'];
      ${$var_name}[$uid]['PATIENTID'] = $id;

      $date = $uid_data['STUDYDATE'];
      $time = $uid_data['STUDYTIME'];
      $dt = DateTime::createFromFormat('Ymd Gis', $date . ' '. $time);

      ${$var_name}[$uid]['DATE']=$date;
      ${$var_name}[$uid]['DATETIME']=$dt;
      ${$var_name}[$uid]['PASS'] = 'pending';

      if(array_key_exists($date, ${$var_map}) &&
         array_key_exists($uid, ${$var_map}[$date]))
      {
        util::out('WARNING: non-unique date UID mapping');
      }
      ${$var_map}[$date][$uid]=$id;

      // add uid to the list of current barcode
      ${$var_barcodemap}[$id][]=$uid;
    }
  }
}

$num_rating_ok_opal_data=array('LEFT'=>0,'RIGHT'=>0);
$num_sr_ok_opal_data=array('LEFT'=>0,'RIGHT'=>0);

$num_rating_incorrect_barcode = array('LEFT'=>0,'RIGHT'=>0);
$num_rating_ok_barcode = array('LEFT'=>0,'RIGHT'=>0);

$num_sr_incorrect_barcode=array('LEFT'=>0,'RIGHT'=>0);
$num_sr_ok_barcode = array('LEFT'=>0,'RIGHT'=>0);

$verbose = false;
$num_rating_date_invalid = array('LEFT'=>0,'RIGHT'=>0);
$num_sr_date_invalid = array('LEFT'=>0,'RIGHT'=>0);
$uid_date_invalid=array();
$index = 1;
foreach($set as $var)
{
  $data_key_set = $data_keys[$var];
  foreach($var_side as $key=>$side)
  {
    util::show_status($index++,4);
    $var_name = $var . '_' . $key;
    foreach(${$var_name} as $uid=>$var_data)
    {
      $opal_id = $opal[$uid]['BARCODE']; // the true visit barcode
      $opal_site = $opal[$uid]['SITE'];
      $begintime = $opal[$uid]['DATETIME'];
      $date_valid = true;
      $site_valid = true;
      $patid = $var_data['PATIENTID']; // this should always match
      $orig_id = $var_data['ORIGINALID'];
      if($patid != $opal_id)
      {
        util::out('ERROR: ' . $uid . ' ' . $var_name . ' data element barcode error');
        die();
      }

      if($patid!=$orig_id)   
       ${'num_'.$var.'_incorrect_barcode'}[$side]++;

      if($orig_id == $opal_id) 
       ${'num_'.$var.'_ok_barcode'}[$side]++;

      // sanity check on datetimes:
      // acquisition datetime should be after minimum of the precedint Onyx interview stages:
      // Contraindications Qnaire, StandingHeight, BloodPressure, Weight
      if($var_data['DATETIME']->format('Ymd') != $begintime->format('Ymd'))
      {
        $d1 = new DateTime($var_data['DATETIME']->format('Y-m-d H:i:s'));
        $d2 = new DateTime($begintime->format('Y-m-d H:i:s'));
        $d1->setTime(0,0);
        $d2->setTime(0,0);
        if($d2 > $d1)
        {
          $date_valid = false;
          $uid_date_invalid[]=$uid; 
        }  
      }
      if(!$date_valid)
        ${'num_'.$var.'_date_invalid'}[$side]++;

      if($var_data['SITE']!=$opal_site)
      {
        // this is a an error that should have been caught by
        // process_ratings.php and process_sr.php
        //
        util::out('ERROR! ' . $uid . ':' . $var_name . ' invalid site ' . $var_data['SITE'] . ' < > ' . $opal_site);
        die();
      }

      $unique = unique_test($uid, ${$var_name}, ${$var_name.'_map'}, $data_keys[$var]);
      if(true!==$unique)
      {
        // this is a an error that should have been caught by
        // process_ratings.php and process_sr.php
        //
        util::out('ERROR! ' . $uid . ':' . $var_name . ' non-unique data shared with ' . count($unique));
        die();
      }

      // otherwise, we have a unique data element
      // determine if it has a valid id
      //
      if($patid == $opal_id)
      {
        ${$var_name}[$uid]['PASS'] = 'ok';
        ${'num_'.$var.'_ok_opal_data'}[$side]++;

        continue;
      }

      // otherwise we have an invalid id

      // bogus barcode ?
      $in_use = array_key_exists($patid, $barcode_to_uid);

      // any other ids ?
      $id_use_count = count(${$var_name.'_barcodemap'}[$patid]);

      if(!$in_use)
      {
        if($id_use_count>1)
        {
          // get the data for this barcode and its sibling uid's
          foreach(${$var_name.'_barcodemap'}[$patid] as $sib_uid)
          {
            if($uid == $sib_uid) continue;
            if($var_data['SITE'] == ${$var_name}[$sib_uid]['SITE'])
            {
              $id_use_count--;
              continue;
            }
            $str = '';
            $match = compare_data($uid,$sib_uid,${$var_name}, $data_keys[$var], $str);
            // this should never happen since the previous process_* scripts assign data
            // uniquely by date and datetime
            if($match)
            {
              util::out('ERROR: ' . $opal_id . ' ' . $uid . '(' . $patid . ') => ' . $sib_uid . '(' . ${$var_name}[$sib_uid]['PATIENTID'] .
                ') : [' . $str . ($match ?  '] MATCH ':'] DIFF ') );
              die();
            }
            $id_use_count--;
          }
        }
        if(1==$id_use_count)
        {
          ${$var_name}[$uid]['PASS'] = 'ok (unknown barcode)';
          ${'num_'.$var.'_ok_opal_data'}[$side]++;
          continue;
        }
        else
        {
          util::out('ERROR: ' . $opal_id . ' ' . $uid . '(' . $patid . ') uniqueness failed on previous process scripts (A) ' . $id_use_count );
          die();
        }
      }

      // if the the id is a valid opal id and
      // this is the only data element with a mapped uid that matches the current data and opal uid
      // accept it as a malformed id
      if($in_use && 1==$id_use_count)
      {
        if($uid!=current(${$var_name.'_barcodemap'}[$patid]))
        {
          util::out('ERROR: ' . $uid . ' uid does not match current barcode' );
          die();
        }
        ${$var_name}[$uid]['PASS'] = 'ok (incorrect barcode)';
        ${'num_'.$var.'_ok_opal_data'}[$side]++;
        continue;
      }

      // then the barcode is valid but does not match the expected opal barcode
      // and there are siblings that share this barcode

      // mark as a failure for now
      //${$var_name}[$uid]['PASS'] = 'fail (unknown): ' . $id . ' < > ' . $opal_id;
      //${'num_'.$var.'_err_barcode'}[$side]++;

      // get the data for this id and its siblings
      foreach(${$var_name.'_barcodemap'}[$patid] as $sib_uid)
      {
        if($uid == $sib_uid) continue;
        if($var_data['SITE'] == ${$var_name}[$sib_uid]['SITE'])
        {
          $id_use_count--;
          continue;
        }

        $str = '';
        $match = compare_data($uid,$sib_uid,${$var_name}, $data_keys[$var], $str);
        // this should never happen since the previous process_* scripts assign data
        // uniquely by date and datetime
        if($match)
        {
          util::out('ERROR: ' . $opal_id . ' ' . $uid . '(' . $patid . ') => ' . $sib_uid . '(' . ${$var_name}[$sib_uid]['PATIENTID'] .
            ') : [' . $str . ($match ?  '] MATCH ':'] DIFF ') );
          die();
        }
        $id_use_count--;
      }
      if(1==$id_use_count)
      {
        ${$var_name}[$uid]['PASS'] = 'ok (incorrect barcode)';
        ${'num_'.$var.'_ok_opal_data'}[$side]++;
      }
      else
      {
        util::out('ERROR: ' . $opal_id . ' ' . $uid . '(' . $patid . ') uniqueness failed on previous process scripts (B) ' . $id_use_count );
        die();
      }
    }
  }
}


foreach($set as $var)
{
  foreach($var_side as $key=>$side)
  {
    util::out('number of ' . $side . ' ' . $var . ' ' . ${'num_'.$var.'_total'}[$side]);
    util::out('number of ' . $side . ' ' . $var . ' accept as correct ' . ${'num_'.$var.'_ok_opal_data'}[$side]);
    util::out('number of ' . $side . ' ' . $var . ' accept with correct barcode ' . ${'num_'.$var.'_ok_barcode'}[$side]);
    util::out('number of ' . $side . ' ' . $var . ' accept with incorrect barcode ' . ${'num_'.$var.'_incorrect_barcode'}[$side]);
    util::out('number of ' . $side . ' ' . $var . ' accept with possibly invalid date ' . ${'num_'.$var.'_date_invalid'}[$side]);
  }
}
util::out('number of UIDs with a possibly invalid date element ' . count(array_unique($uid_date_invalid)));
// write the final data out


$header_keys = array(
'ZE',
'AR',
'AS',
'NA',
'NO',
'PI',
'AN',
'CE',
'CS',
'PC',
'NI',
'SB',
'SK',
'GB',
'LO',
'AM',
'ME',
'RATING',
'QUALITY',
'PATIENTID_STILL',
'ORIGINALID_STILL',
'STUDYDATE_STILL',
'STUDYTIME_STILL',
'AVG',
'MAX',
'MIN',
'STD',
'NMEAS',
'PATIENTID_SR',
'ORIGINALID_SR',
'STUDYDATE_SR',
'STUDYTIME_SR'
);

$rating_keys = array(
'ZE',
'AR',
'AS',
'NA',
'NO',
'PI',
'AN',
'CE',
'CS',
'PC',
'NI',
'SB',
'SK',
'GB',
'LO',
'AM',
'ME',
'RATING',
'QUALITY',
'PATIENTID',
'ORIGINALID',
'STUDYDATE',
'STUDYTIME'
);

$sr_keys = array(
      'IMT_Posterior_Average',
      'IMT_Posterior_Max',
      'IMT_Posterior_Min',
      'IMT_Posterior_SD',
      'IMT_Posterior_nMeas',
'PATIENTID',
'ORIGINALID',
'STUDYDATE',
'STUDYTIME'
      );

$default_rating = array_combine($rating_keys,array_fill(0,count($rating_keys),0));
$default_sr = array_combine($sr_keys,array_fill(0,count($sr_keys),''));
$outFile = 'cIMT_BL_final_output.csv';
$file = fopen($outFile, 'w');

$header = array('entity_id');
foreach($var_side as $key=>$side)
{
  $header[] = 'IMT_' . strtoupper($side) . '_COM';
  foreach($header_keys as $hkey)
    $header[] = 'IMT_' . $key . '_' . $hkey . '_COM';
}
$str = '"' . util::flatten($header,'","') . '"' . PHP_EOL;
fwrite($file,$str);
$num_date_err=0;
$num_datetime_err=0;
$num_datetime_with_barcode_err=0;
$num_final_row = 0;
$num_sr_rated_L = 0;
$num_sr_rated_R = 0;
$num_rated_no_sr_L = 0;
$num_rated_no_sr_R = 0;
$num_sr_no_rated_L = 0;
$num_sr_no_rated_R = 0;
$uid_sr_no_rating = array();
$num_barcode_err_by_site = array_combine($site_names, array_fill(0,count($site_names),0));
$num_element_by_site = array_combine($site_names, array_fill(0,count($site_names),0));

$index=1;
foreach($opal as $uid=>$opal_data)
{
  util::show_status($index++,$num_uid);
  $str = array();
  $str[]=$uid;
  $has_R=false;
  $has_L=false;
  unset($rating_data);
  unset($sr_data);
  $rating_data['R']=$default_rating;
  $rating_data['L']=$default_rating;
  $sr_data['R']=$default_sr;
  $sr_data['L']=$default_sr;
  $date_set['R'] = array();
  $date_set['L'] = array();
  $datetime_set['R'] = array();
  $datetime_set['L'] = array();
  $barcode_set['R'] = array();
  $barcode_set['L'] = array();
  $num_rating_L = 0;
  $num_rating_R = 0;
  $num_sr_L = 0;
  $num_sr_R = 0;
  $site = $opal_data['SITE'];
  foreach($set as $var)
  {
    foreach($var_side as $key=>$side)
    {
      $var_name = $var . '_' . $key;
      if(array_key_exists($uid,${$var_name}))
      {
        ${'num_' . $var . '_' . $key}++;
        $num_element_by_site[$site]++;

        $var_data = ${$var_name}[$uid];
        // sanity check: the cIMT image and the SR data come from the same day
        // but not necessarily the same time?
        $date_set[$key][] = $var_data['STUDYDATE'];
        $datetime_set[$key][] = $var_data['STUDYDATE'] . $var_data['STUDYTIME'];
        $barcode_set[$key][] = $opal_data['BARCODE']==$var_data['ORIGINALID'] ? 'match' : 'diff';

        ${'has_'.$key} = true;
        if($var=='rating')
        {
          $codes = array_map('trim',explode(',',$var_data['CODE']));
          foreach($codes as $code)
          {
            $rating_data[$key][$code]=1;           
          }
          $rating_data[$key]['QUALITY']=$var_data['QUALITY'];
          $rating_data[$key]['RATING']=$var_data['RATING'];
          $rating_data[$key]['PATIENTID']=$var_data['PATIENTID'];
          $rating_data[$key]['ORIGINALID']=$var_data['ORIGINALID'];
          $rating_data[$key]['STUDYDATE']=$var_data['STUDYDATE'];
          $rating_data[$key]['STUDYTIME']=$var_data['STUDYTIME'];
        }
        else
        {
          foreach($var_data as $id=>$value)
          {
            if(in_array($id,$sr_keys))
            {
              $sr_data[$key][$id]=$value;
            }
          }    
        }
      }
    }
  }

  if(1 == $num_sr_L && 1 == $num_rating_L) $num_sr_rated_L++;
  if(1 == $num_sr_R && 1 == $num_rating_R) $num_sr_rated_R++;
  if(0 == $num_sr_L && 1 == $num_rating_L) $num_rated_no_sr_L++;
  if(0 == $num_sr_R && 1 == $num_rating_R) $num_rated_no_sr_R++;
  if(1 == $num_sr_L && 0 == $num_rating_L) 
  {
    $num_sr_no_rated_L++;
    $uid_sr_no_rating[]=$uid;
  }  
  if(1 == $num_sr_R && 0 == $num_rating_R) 
  {
    $num_sr_no_rated_R++;
    $uid_sr_no_rating[]=$uid;
  }  

  // sanity check that data for the current side came from the same day
  $date_err=false;
  $rating_datetime = array();
  $sr_datetime = array();
  foreach($var_side as $key=>$side)
  {    
    $has_barcode_err = in_array('diff',array_unique($barcode_set[$key]));

    $str [] = ${'has_'.$key} ? 1 : 0;
    foreach($rating_data[$key] as $id=>$value)
      $str[]=$value;
    foreach($sr_data[$key] as $id=>$value)
      $str[]=$value;
    $nd = count($date_set[$key]);
    $nu = count(array_unique($date_set[$key]));
    if($nu>1)
    {
      util::out('WARNING! ' . $uid . ' has ' . $nu .  ' non-unique dates of ' . $nd . '['. util::flatten($date_set[$key]));
      $date_err=true;
    }  
    $nd = count($datetime_set[$key]);
    $nu = count(array_unique($datetime_set[$key]));
    if($nd > 0 && $nu != 1)
    {
      //util::out('WARNING! ' . $uid . ' has ' . $nu .  ' non-unique datetimes of ' . $nd . '['. util::flatten($datetime_set[$key]));
      // check if the barcode and patient id tags match
      $num_datetime_err++;
      
      // if any of the data elements do not have perfect barcode to patientid matches
      if($has_barcode_err)
        $num_datetime_with_barcode_err++;
    }
    if($has_barcode_err)
      $num_barcode_err_by_site[$site]++;
  }  
  if($date_err)
    $num_date_err++;

  if($has_L || $has_R)
  {
    $str = '"' . util::flatten($str,'","') . '"' . PHP_EOL;
    fwrite($file,$str);
    $num_final_row++;
  }  
}
util::out('number of sr rated left ' . $num_sr_rated_L);
util::out('number of sr rated right ' . $num_sr_rated_R);
util::out('number of ratings no sr left ' . $num_rated_no_sr_L);
util::out('number of ratings no sr right ' . $num_rated_no_sr_R);
util::out('number of sr no rating left ' . $num_sr_no_rated_L);
util::out('number of sr no rating right ' . $num_sr_no_rated_R);
util::out('number of UIDs with srs having no rating ' . count(array_unique($uid_sr_no_rating)));

util::out('number of UIDs with one or more data elements ' . $num_final_row);
util::out($num_date_err . ' possible date errors among ' . $num_final_row . ' out of a maximim of ' . count($opal));
util::out($num_datetime_err . ' possible datetime errors among ' . $num_final_row . ' out of a maximim of ' . count($opal));
util::out($num_datetime_with_barcode_err . ' possible datetime errors due to ownership (barcode) errors among ' . $num_datetime_err);

foreach($num_barcode_err_by_site as $site=>$num)
{
  $total = $num_element_by_site[$site];
  util::out($num . ' / ' . $total . ' (' . round(100.0*$num/$total,2) . '%) barcode errors at '. $site );
}

util::out('data written to ' . $outFile);
fclose($file);
