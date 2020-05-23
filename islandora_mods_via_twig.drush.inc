<?php

/**
 * @file
 * islandora_mods_via_twig (imvt)
 */

/**
 * Implements hook_drush_command().
 *
 * In this hook, you specify which commands your drush module makes available, what it does and
 * description.
 *
 * See `drush topic docs-commands` for a list of recognized keys.
 */
function islandora_mods_via_twig_drush_command() {
  $commands = array();

  // The 'islandora_mods_via_twig' command.
  $commands['islandora_mods_via_twig'] = array(
    'description' => dt('Generate MODS .xml files from the mods-imvt.tsv file for a specified collection.'),
    'drupal dependencies' => array(
      'islandora',
    ),
    'arguments' => array(
      'collection' => dt('The name of the collection to be processed.  Defaults to "social-justice".'),
    ),
    'aliases' => array('imvt'),
    'examples' => array(
      'drush -u 1 islandora_mods_via_twig social-justice' => dt('Process ../social-justice/mods-imvt.tsv.'),
    ),
    'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_LOGIN,
    // 'bootstrap' => 'DRUSH_BOOTSTRAP_FULL',  // No bootstrap at all?
  );

  // The 'islandora_mods_post_processing' command.
  $commands['islandora_mods_post_processing'] = array(
    'description' => dt("Find a collection's ./ready-for-datastream-replace/*.used files and comment out found PIDs from all *.tsv files."),
    'drupal dependencies' => array(
      'islandora',
    ),
    'arguments' => array(
      'collection' => dt('The name of the collection to be examined for *.used files.  Defaults to "social-justice".'),
    ),
    'aliases' => array('impp'),
    'examples' => array(
      'drush -u 1 islandora_mods_post_processing social-justice' => dt('Process ../social-justice/ready-for-database-replace/*.used files.'),
    ),
    'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_LOGIN,
    // 'bootstrap' => 'DRUSH_BOOTSTRAP_FULL',  // No bootstrap at all?
  );

  return $commands;
}

/**
 * Implements drush_hook_COMMAND_validate().
 *
 * The validate command should exit with
 * `return drush_set_error(...)` to stop execution of
 * the command.  In practice, calling drush_set_error
 * OR returning FALSE is sufficient.  See drush.api.php
 * for more details.
 */
function drush_islandora_mods_via_twig_islandora_mods_via_twig_validate() { return TRUE; }

/**
 * Implements drush_hook_COMMAND().
 *
 * The command callback is where the action takes place.
 *
 * The function name should be same as command name but with dashes turned to
 * underscores and 'drush_commandfile_' prepended, where 'commandfile' is
 * taken from the file 'commandfile.drush.inc', which in this case is
 * 'mods_via_twig'. Note also that a simplification step is also done in instances
 * where the commandfile name is the same as the beginning of the command name,
 * "drush_example_example_foo" is simplified to just "drush_example_foo".
 * To also implement a hook that is called before your command, implement
 * "drush_hook_pre_example_foo".  For a list of all available hooks for a
 * given command, run drush in --debug mode.
 *
 * If for some reason you do not want your hook function to be named
 * after your command, you may define a 'callback' item in your command
 * object that specifies the exact name of the function that should be
 * called.
 *
 * In this function, all of Drupal's API is (usually) available, including
 * any functions you have added in your own modules/themes.
 *
 * @see drush_invoke()
 * @see drush.api.php
 */
function drush_islandora_mods_via_twig($target_collection = 'social-justice') {

  define('SEPERATOR', "\t");
  // define('TWIG', './sites/default/files/collections/Digital-Grinnell-MODS-Master-Rev18.twig');
  // define('COLLECTIONS_PATH', './sites/default/files/collections/');
  define('TWIG', '/mnt/metadata-review/Digital-Grinnell-MODS-Master.twig');
  define('COLLECTIONS_PATH', '/mnt/metadata-review');
  define('XML_OUTPUT_PATH', '/ready-for-datastream-replace');
  define('STORAGE_PATH', '//STORAGE/LIBRARY/ALLSTAFF/DG-Metadata-Review-2020-r1');

  // Note that the TWIG and COLLECTIONS_PATH defined above depend on the following mount/mapping on DGDocker1...
  //  docker exec -it isle-apache-dg bash
  //  mount -t cifs -o username=mcfatem /storage.grinnell.edu/LIBRARY/ALLSTAFF/DG-Metadata-Review-2020-r1 /mnt/metadata-review

  echo "\n\n\nWelcome to drush islandora_mods_via_twig!\n";

  // Twig handling is based on https://stackoverflow.com/questions/24888372/drupal-8-render-twig-from-string-with-twig-functions

  // read the Twig template into a string and remove all "data." variable prefixes
  // from the raw Twig template string
  $TwigString = file_get_contents(TWIG);
  if (!$TwigString) {
    return drush_set_error('Failed to Read TWIG', dt("The specified Twig template, ".TWIG." could not be read."));
  }

  // create a new Twig environment
  $Twig = new \Twig_Environment(new \Twig_Loader_String());
  $Twig->setLoader(new \Twig_Loader_String());
  // $Twig->addExtension(new Jasny\Twig\PcreExtension());  // lifted from IMI utilities.inc

  // check the COLLECTIONS_PATH, it is a directory?
  if (!is_dir(COLLECTIONS_PATH)) {
    return drush_set_error('Invalid COLLECTIONS_PATH', dt("The specified collections path, ".COLLECTIONS_PATH.", is not a valid directory."));
  }

  // loop through all the directories in COLLECTIONS_PATH
  $paths = scandir(COLLECTIONS_PATH);   // echo "$paths: ", print_r($paths, TRUE);

  foreach ($paths as $collection) {    // drush_print("Now processing $collection.");
    if ($collection == $target_collection && $collection != '.' && $collection != '..' && is_dir(COLLECTIONS_PATH."/".$collection)) {
      drush_print("Now processing the collection in: $collection.");
      // look for THE mods-imvt.tsv file inside
      $file = COLLECTIONS_PATH . "/$collection/mods-imvt.tsv";
      if (!file_exists($file)) {
        return drush_set_error('No mods-imvt.tsv file found', dt("The $collection directory contains NO mods-imvt.tsv file."));
      }

      // process the found .tsv file in $collection
      // read the .tsv exploded into an array of $lines
      $lines = explode("\n", file_get_contents($file));  // echo "$lines: ", print_r($lines, TRUE);

      // break out the $headers, assuming SEPERATOR, into an array, and create an empty $data array
      $headers = str_getcsv(array_shift( $lines ), SEPERATOR);  // echo "headers: \n", print_r($headers, TRUE), "\n\n";
      $data = array();

      // loop on the .tsv line-by-line, one object at a time
      foreach ($lines as $line) {
        if (strlen($line) > 0) {    // skip any blank rows in the .tsv

          // take care of any unexcapsed &, smart quotes, or other squirrely characters!
          $noSmartQuotes = iconv('UTF-8', 'ASCII//TRANSLIT', $line);
          $line = htmlspecialchars($noSmartQuotes, ENT_NOQUOTES, 'UTF-8', false);

          $row = array();
          foreach (str_getcsv($line, SEPERATOR) as $key => $field ) {
            // echo "key => field: " . print_r($key, TRUE) . " => " . print_r($field, TRUE) . "\n";
            $row[ strtolower($headers[ $key ]) ] = trim($field);    // make sure we convert headers (keys) to lowercase!
          }

          $data = array_filter( $row );  // echo "data: \n", print_r($data, TRUE), "\n";

          $pid = $data['pid'];    // fetch the PID and check it for proper form
          $pos = strpos($pid, "#");
          if ($pos === 0) {       // found a comment row...skip it!
            echo "Skipping comment row that begins with... '$pid'\n";
            continue;
          }

          $pos = strpos($pid, "grinnell:");
          if ($pos === false || $pos !== 0) {
            echo "data: \n", print_r($data, TRUE), "\n";
            return drush_set_error('Bad PID', dt("An invalid PID was encountered. Process terminated."));
          }

          if (!isset($data['import_source'])) {
            $data['import_source'] = $files[0];  // push the .tsv path into dg_importSource if that field is empty
          }

          // make the .xml output directory path if it does not already exist
          $xml_dir = COLLECTIONS_PATH . "/$collection" . XML_OUTPUT_PATH;
          if (!file_exists($xml_dir)) {
              mkdir($xml_dir, 0777, true);
          }

          $xml_path = "$xml_dir/$xml" . str_replace(":", "_", $pid) . "_MODS.xml";
          $imi_like_data['data'] = $data;
          $rendered = $Twig->render($TwigString, $imi_like_data);  // echo "rendered: \n".$rendered."\n\n";

          // pretty-print the $rendered xml to a pid-named file in the $collection directory
          $dom = new DOMDocument('1.0','UTF-8');
          $dom->formatOutput = true;
          $dom->preserveWhiteSpace = false;
          $dom->loadXML($rendered);
          $dom->save($xml_path) or die('XML Create Error');
        }
      }    // end: foreach($lines as $line)...
    }    // end: if (is_dir($collection)...
  }    // end: foreach (scandir(COLLECTIONS_PATH)...

  echo "\n\ndrush islandora_mods_via_twig is finished!\n\n";

}

/**
 * Implements drush_hook_COMMAND().
 *
 * The command callback is where the action takes place.
 *
 * The function name should be same as command name but with dashes turned to
 * underscores and 'drush_commandfile_' prepended, where 'commandfile' is
 * taken from the file 'commandfile.drush.inc', which in this case is
 * 'mods_via_twig'. Note also that a simplification step is also done in instances
 * where the commandfile name is the same as the beginning of the command name,
 * "drush_example_example_foo" is simplified to just "drush_example_foo".
 * To also implement a hook that is called before your command, implement
 * "drush_hook_pre_example_foo".  For a list of all available hooks for a
 * given command, run drush in --debug mode.
 *
 * If for some reason you do not want your hook function to be named
 * after your command, you may define a 'callback' item in your command
 * object that specifies the exact name of the function that should be
 * called.
 *
 * In this function, all of Drupal's API is (usually) available, including
 * any functions you have added in your own modules/themes.
 *
 * @see drush_invoke()
 * @see drush.api.php
 */
function drush_islandora_mods_via_twig_islandora_mods_post_processing($target_collection = 'social-justice') {

  define('SEPERATOR', "\t");
  // define('TWIG', './sites/default/files/collections/Digital-Grinnell-MODS-Master-Rev18.twig');
  // define('COLLECTIONS_PATH', './sites/default/files/collections/');
  define('TWIG', '/mnt/metadata-review/Digital-Grinnell-MODS-Master.twig');
  define('COLLECTIONS_PATH', '/mnt/metadata-review');
  define('XML_OUTPUT_PATH', '/ready-for-datastream-replace');
  define('STORAGE_PATH', '//STORAGE/LIBRARY/ALLSTAFF/DG-Metadata-Review-2020-r1');

  // Note that the TWIG and COLLECTIONS_PATH defined above depend on the following mount/mapping on DGDocker1...
  //  docker exec -it isle-apache-dg bash
  //  mount -t cifs -o username=mcfatem /storage.grinnell.edu/LIBRARY/ALLSTAFF/DG-Metadata-Review-2020-r1 /mnt/metadata-review

  drush_print("\n\n\nWelcome to drush islandora_mods_post_processing!\n");

  // check the COLLECTIONS_PATH, it is a directory?
  if (!is_dir(COLLECTIONS_PATH)) {
    return drush_set_error('Invalid COLLECTIONS_PATH', dt("The specified collections path, ".COLLECTIONS_PATH.", is not a valid directory."));
  }

  // make a list (array) of all the .tsv files in COLLECTIONS_PATH
  $tsv_list = iterator_to_array(
    new GlobIterator(COLLECTIONS_PATH . "/*/*.tsv", GlobIterator::CURRENT_AS_PATHNAME) );
  $count = count($tsv_list);
  drush_print("The collections contain $count .tsv files to check and modify.");

  // loop through all the directories in COLLECTIONS_PATH
  $paths = scandir(COLLECTIONS_PATH);   // echo "$paths: ", print_r($paths, TRUE);

  foreach ($paths as $collection) {    // drush_print("Now processing $collection.");
    if ($collection == $target_collection && $collection != '.' && $collection != '..' && is_dir(COLLECTIONS_PATH."/".$collection)) {
      drush_print("Now processing the collection in: $collection.");
      // look for .used files in THE ./ready-for-datastream-replace directory inside
      $it = iterator_to_array(
        new GlobIterator(COLLECTIONS_PATH . "/$collection/ready-for-datastream-replace/*.used", GlobIterator::CURRENT_AS_PATHNAME) );
      $count = count($it);
      if ($count == 0) {
        return drush_set_error('No .used files found', dt("The $collection directory contains NO ./ready-for-datastream-replace/*.used files."));
      }

      $pid_list = array( );
      $idx = 0;

      // loop through all the found .used filenames...  build a list of .used PIDs
      foreach ($it as $used) {
        $filename = basename($used);
        $blast = explode('_', $filename, 3);
        $pid = $blast[0] . ':' . $blast[1];
        $pidx = $blast[0] . '::' . $blast[1];
        $pid_list[$idx++] = $pid;
        $pattern = "/^$pid *\t/m";   // match only at the start of a new line!
        $dt = date(DATE_RFC822);
        $replacement = "# $pidx - reviewed and modified at $dt as part of $target_collection \t";
        $replaced = 0;
        drush_print("Now processing PID: '$pid', pattern: '$pattern'");

        // inner loop through all the gathered .tsv files
        foreach ($tsv_list as $path) {
          drush_print("  Checking $path for $pid...");
          $tsv_contents = file_get_contents($path);
          // if $pattern is found
          if (preg_match($pattern, $tsv_contents)) {
            drush_print("    Found $pid in $path.");
            $new_contents = preg_replace($pattern, $replacement, $tsv_contents, 1, $replaced);   // only do ONE replacement!
            if ($replaced > 0) {
              drush_print("      Executed $replaced replacement(s).  New fields say: '$replacement'");
              file_put_contents($path, $new_contents);
              drush_print("    Replacement saved to $path.");
            } else {
              drush_print("    Nothing to replace.  Moving on.");
            }
          }
        }
      }
    }

  }

  echo "\n\ndrush islandora_mods_post_processing is finished!\n\n";

}
