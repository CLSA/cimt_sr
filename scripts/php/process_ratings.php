<?php

require_once('/home/dean/files/repository/php_util/util.class.php');
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

// read the file
if(3 != count($argv))
{
  util::out('usage: process_ratings input.csv output.csv');
  exit;
}

// we need the mapping from UID to true barcode from opal
$opalFile = 'OPAL_BL_MAP.csv';
$indata = read_csv($opalFile);
$opal_barcode_to_uid = array();
$opal_uid_to_barcode = array();
$opal_data = array();
foreach($indata as $data)
{
  $uid = $data['UID'];
  $opal_barcode_to_uid[$data['BARCODE']]=$uid;
  $opal_uid_to_barcode[$uid]=$data['BARCODE'];
  if($data['SITE']=='McMaster') $data['SITE']='Hamilton';
  $data['DATETIME']=
    DateTime::createFromFormat('Ymd H:i:s', $data['BEGIN']);
  $opal_data[$uid] = $data;
}

// INPUT a raw csv Alder rating report for cIMT images
//
$infile = $argv[1];
$outfile = $argv[2];
$indata = read_csv($infile);
$wave = 'Baseline';
$expert = 'lisa';
$data = array('left'=>array(),'right'=>array());
$index = 1;
$num_row = count($indata);
$num_data = 0;
/*
$site_keys = array();
foreach($indata as $row)
{
  if($wave != $row['WAVE'] ||
     'NA' == $row['DERIVED'])
    continue;
  $site_keys[strtoupper(substr($row['SITE'],0,3))];
}
$site_keys=array_keys($site_keys);
sort($site_keys);
var_dump($site_keys);
die();
*/
$num_reject_datetime = 0;
$num_missing_barcode = 0;
$num_valid_barcode = 0;
$bogus_id = array();
$repair_id = array();
foreach($indata as $row)
{
  util::show_status($index++,$num_row);
  // discard on wave
  // discard on derived=NA
  if($wave != $row['WAVE'] ||
     'NA' == $row['DERIVED'])
  {
    continue;
  }

  // group by UID, side, compute quality value based on codes, compute
  // length of code favoring longer (more verbose) codes
  $uid = $row['UID'];
  $code = $row['CODE'];
  $rating = $row['DERIVED'];
  $rater = $row['RATER'];
  $side = $row['SIDE'];
  $wv = $row['WAVE'];
  $site = $row['SITE'];
  if($site=='McMaster') $site = 'Hamilton';
  $visit = str_replace('-','',$row['VISITDATE']);
  $path = $row['PATH'];
  $patid = $row['PATIENTID'];
  if(''==$patid || 'NA'==$patid)
  {
    $patid = $opal_uid_to_barcode[$uid];
    $num_missing_barcode++;
  }
  $stdate = $row['STUDYDATE'];
  if(''==$stdate || 'NA'==$stdate)
  {
    //util::out('WARNING: ' . $uid . ' missing study date [' . $row['PATH'] .']');
    $num_reject_datetime++;
    continue;
  }
  $sttime = $row['STUDYTIME'];
  if(''==$sttime || 'NA'==$sttime)
  {
    //util::out('WARNING: ' . $uid . ' missing study time [' . $row['PATH'] .']');
    $num_reject_datetime++;
    continue;
  }

  if(5 == strlen($sttime)) $sttime = '0' . $sttime;
  $site_key = strtoupper(substr($site,0,3));

  // it is possible study datetime is not unique among sites, add a 3 char site key
  $dt_key = $stdate . $sttime . $site_key;

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

  $rater_data=
    array(
      'UID'=>$uid,
      'RATING'=>$rating,
      'CODE'=>$code,
      'QUALITY'=>get_quality($code),
      'LENGTH'=>count(explode(',',trim($code))),
      'PATIENTID'=>$patid,
      'STUDYDATE'=>$stdate,
      'STUDYTIME'=>$sttime,
      'SITE'=>$site,
      'RATER'=>$rater,
      'SIDE'=>$side,
      'VALID'=>($patid == $opal_uid_to_barcode[$uid] && $site == $opal_data[$uid]['SITE']),
      'DATETIME'=>(DateTime::createFromFormat('Ymd Gis', $stdate . ' ' . $sttime)),
      'ORIGINALID'=>$orig_id,
      'BARCODE'=>$opal_uid_to_barcode[$uid],
      'MATCH'=>($opal_uid_to_barcode[$uid]==$orig_id),
      'PATH'=>$path
      );

  if($patid == $opal_uid_to_barcode[$uid]) $num_valid_barcode++;
  $datetime_to_rater[$side][$dt_key][] = $rater_data;
  $num_data++;
}

util::out('found ' . $num_data . ' ratings out of ' . $num_row . ' rows');
util::out('rejected datetimes ' . $num_reject_datetime . ' out of ' . $num_row );
util::out('number of missing barcodes ' . $num_missing_barcode . ' out of ' . $num_row );
util::out('number of bogus barcodes ' . count(array_unique($bogus_id)));
util::out('number of repaired barcodes ' . count(array_unique($repair_id)));
util::out('number of valid barcodes ' . $num_valid_barcode);
$num_image_left = count($datetime_to_rater['left']);
$num_image_right = count($datetime_to_rater['right']);
$num_image_total = $num_image_left + $num_image_right;
util::out('found ' . $num_image_left . ' left images rated of ' . $num_image_total);
util::out('found ' . $num_image_right . ' right images rated of ' . $num_image_total);
util::out('estimated number of duplicate ratings ' . ($num_data - $num_image_total));

// number of images that were exported under more than one participant id
$num_id_duplicate=0;
// get a sample of valid uid barcode image for each site
$valid_data=array();
$num_valid_rating = 0;
$num_valid_image = 0;
$overwrite_keys = array('UID','SITE','PATIENTID','ORIGINALID','BARCODE');
$num_side_match=0;
foreach($datetime_to_rater as $side=>$side_data)
{
  foreach($side_data as $datetimesite_key=>$rater_data)
  {
    // check if this data is duplicate of opposite side
    $other_side = $side == 'left' ? 'right' : 'left';
    if(array_key_exists($datetimesite_key,$datetime_to_rater[$other_side]))
    {
      //util::out('WARNING: sides match for key ' . $datetimesite_key);
      // are we expecting both sides?

      // get the UIDs and file paths that match on each site
      $this_data = $rater_data;
      $that_data = $datetime_to_rater[$other_side][$datetimesite_key];
      $this_path = array();
      $that_path = array(); 
      $this_uid =array();
      $that_uid =array();
      foreach($this_data as $data)
      {
        $this_path[] = $data['PATH'];
        $this_uid[] = $data['UID'];
      }
      $this_path = array_unique($this_path);
      $this_uid = array_unique($this_uid);
      foreach($that_data as $data)
      {
        $that_path[] = $data['PATH'];
        $that_uid[] = $data['UID'];
      }
      $that_path = array_unique($that_path);
      $that_uid = array_unique($that_uid);
      util::out('WARNING: sides match ' . $other_side . ':' . util::flatten($that_path) . ' ' . $side . ':' . util::flatten($this_path));
      $num_side_match++;
      continue;
    }  

    // for each unique image in terms of its datetime/site key
    // determine the true opal UID barcode site ownership
    $patientid_list=array();
    $uid_list=array();
    $direct_match_idx = false;
    foreach($rater_data as $idx_rater=>$data)
    {
      $patientid_list[$idx_rater]=$data['PATIENTID'];
      $uid_list[$idx_rater]=$data['UID'];
      if($data['MATCH'] && false == $direct_match_idx)
        $direct_match_idx = $idx_rater;
    }
    // get the list of data indexes unique to the cleaned barcode for this data item
    $unique = array_unique($patientid_list);
    if(1 != count($unique))
    {
      util::out('ERROR: multiple barcodes for same image');
      die();
    }
    $unique = array_unique($uid_list);
    if(1 != count($unique))
    {
      $num_id_duplicate++; // number of images rated under different uid's
      $owner_idx = $direct_match_idx;

      // can we assign the correct id?
      // determine the true id uid owner of this image data
      // by comparing the dicom datetime to the opal estimated datetime
      // of the earliest event prior to acquisition
      if(false===$owner_idx)
      {
        $valid_time=array();
        foreach($patientid_list as $idx_rater => $patientid)
        {
          $data = $rater_data[$idx_rater];
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
            $valid_time[$idx_rater] = $diff;
            // the last valid id just in case there is only 1 valid time
            $owner_idx = $idx_rater;
          }
        }

        if(count($valid_time)>1 && asort($valid_time))
        {
          // if all times are negative then none of uid barcode pairs could rightfully claim ownership
          // determine the minimum positive time difference
          $owner_idx = false;
          foreach($valid_time as $idx_rater => $t)
          {
            if($t<0) continue;
            $owner_idx = $idx_rater;
            break;
          }
        }
      }
      if(false===$owner_idx)  // no valid uid id pair could be found in the data
      {
        util::out('indeterminate non-unique ' . util::flatten($patientid_list) . ' ' . util::flatten($uid_list) );
        // discard this data as orphaned
      }
      else
      {
        $owner_data = $rater_data[$owner_idx];
        $uid = $owner_data['UID'];
        $dt1 = $owner_data['DATETIME']; // the image date
        $dt2 = $opal_data[$uid]['DATETIME'];
        if($owner_data['PATIENTID']!=$owner_data['BARCODE'])
        {
          util::out($uid. ': ' . $owner_data['PATIENTID'] . ' <= ' . $owner_data['BARCODE']);
          if($owner_data['PATIENTID']!=$owner_data['ORIGINALID'])
            $owner_data['ORIGINALID']=$owner_data['PATIENTID'];
          $owner_data['PATIENTID'] = $owner_data['BARCODE'];
        }

        // mark all the data as belonging to this uid barcode pair
        // overwrite the
        $overwrite_keys = array('UID','SITE','PATIENTID','ORIGINALID');
        $did_overwrite = false;
        foreach($rater_data as $idx_rater=>$data)
        {
          // assign the owner uid and barcode to all ratings of this image
          if($idx_rater!=$owner_idx)
          {
            foreach($overwrite_keys as $key)
            {
              if($data[$key] != $owner_data[$key] && false==$did_overwrite)$did_overwrite = true;
              $data[$key] = $owner_data[$key];
            }
          }
          $valid_data[$side][$datetimesite_key][] = $data;
          $num_valid_rating++;
        }
        util::out('assigning unique ownership ' .
          $uid . ' (' . $owner_data['BARCODE'] . ') ' .
          $dt2->format('Y-m-d H:i:s') . ' > ' . $dt1->format('Y-m-d H:i:s') . ' valid owner: ' . ($owner_data['VALID'] ? 'yes':'no') .
          ' corrections: ' . ($did_overwrite ? 'yes':'no'));
        $num_valid_image++;
      }
    } // end on non-unique clean barcodes
    else
    {
      // all of this data uses the same barcode and uid
      // how many are valid ?
      $owner_idx = $direct_match_idx;
      if(false===$owner_idx)
      {
        // are any of the data valid partially?
        foreach($rater_data as $idx_rater=>$data)
        {
          if($data['VALID'])
          {
            $owner_idx = $idx_rater;
            break;
          }
        }
      }
      if(false===$owner_idx)
      {
        $owner_idx = 0;
        $max_match1 = -1;
        $idx1 = 0;
        $match1 = array();
        $max_match2 = -1;
        $idx2 = 0;
        $match2 = array();
        foreach($rater_data as $idx_sr=>$data)
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
        if(!array_key_exists($rater_data[$idx1]['PATIENTID'], $opal_barcode_to_uid))
        {
          if($max_match1 > 6)
          {
            $owner_idx = $idx1;
            util::out('WARNING: accepting ' . count($rater_data) . ' ratings for indeterminately owned image for ' . 
              $rater_data[$idx1]['UID'] . ' match ' . $match1[$idx1] . ' ' . $rater_data[$idx1]['PATIENTID'] . ' ' . $rater_data[$idx1]['BARCODE']);
          }
          else if($max_match2 > 0)
          {
            $owner_idx = $idx2;
            util::out('WARNING: accepting ' . count($rater_data) . ' ratings for indeterminately owned image for ' . 
              $rater_data[$idx2]['UID'] . ' match ' . $match2[$idx2] . ' ' . $rater_data[$idx2]['PATIENTID'] . ' ' . $rater_data[$idx2]['BARCODE']);
          }
        }

        if(false===$owner_idx)
        {
          util::out('WARNING: cant find a valid owner for ' . count($rater_data) . ' ratings, assigning default');
          $owner_idx = 0;          
        }
      }

      // NOTE: regardless of ownership the image was rated and so we store the ratings
      //
      if(false===$owner_idx)
      {
        util::out('ERROR: cant find a valid owner for ' . count($rater_data) . ' ratings');
        die();
      }  
      else
      {
        $owner_data = $rater_data[$owner_idx];
        if($owner_data['PATIENTID']!=$owner_data['BARCODE'])
        {
          util::out($uid. ': ' . $owner_data['PATIENTID'] . ' <= ' . $owner_data['BARCODE']);
          if($owner_data['PATIENTID']!=$owner_data['ORIGINALID'])
            $owner_data['ORIGINALID']=$owner_data['PATIENTID'];
          $owner_data['PATIENTID'] = $owner_data['BARCODE'];
        }
        foreach($rater_data as $idx_rater=>$data)
        {
          $data['PATIENTID']=$owner_data['PATIENTID'];
          $data['ORIGINALID']=$owner_data['ORIGINALID'];
          $data['BARCODE']=$owner_data['BARCODE'];
          $valid_data[$side][$datetimesite_key][] = $data;
          $num_valid_rating++;
        }
      }
      $num_valid_image++;
    }
  }
}
util::out('number of duplicate image exports ' . $num_id_duplicate);
util::out('number of bogus barcode ids ' . count(array_unique($bogus_id)));
util::out('number of repaired barcode ids ' . count(array_unique($repair_id)));
util::out('number of valid ratings ' . $num_valid_rating);
util::out('number of valid images ' . $num_valid_image);
util::out('number of matched side images ' . $num_side_match);

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

// for UID, side repeats -
// 1) if designated expert user X, keep and discard others
// 2) if code string are identical, keep and discard others
// 3) otherwise, keep highest quality value ("1"==highest, "3"==lowest) and then most verbose (greatest number of codes) as conservative


// go through all ratings by side
// check date time uniqueness to patientID:

$index = 1;
foreach($valid_data as $side=>$side_data)
{
  foreach($side_data as $datetime_key=>$data)
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
    util::show_status($index++,$num_valid_image);
  }
}

fclose($file);

?>
