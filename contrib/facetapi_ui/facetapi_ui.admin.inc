<?php

/**
 * @file
 *
 */

/**
 * Facet settings.
 */
function facetapi_ui_realm_settings_form($form, &$form_state, $searcher, $realm_name) {

  // instantiates adapter, loads realm.
  $adapter = facetapi_adapter_load($searcher);
  $realm = facetapi_realm_load($realm_name);
  $form['#facetapi'] = array(
    'adapter' => $adapter,
    'realm' => $realm,
    'facet_info' => facetapi_get_facet_info($searcher),
  );

  $form['description'] = array(
    '#markup' => filter_xss_admin($realm['description']),
  );

  $form['table'] = array(
    '#theme' => 'facetapi_ui_realm_settings_table',
    '#facetapi' => &$form['#facetapi'],
  );

  // Builds "enabled_facets" options.
  $options = $default_value = array();
  foreach ($form['#facetapi']['facet_info'] as $facet_name => $facet) {
    $settings = $adapter->getFacet($facet)->getSettings($realm);

    // Gets weight, appends to facet_info array for sorting.
    $form['#facetapi']['facet_info'][$facet_name]['weight'] = $settings->settings['weight'];

    $form['table']['weight:' . $facet_name] = array(
      '#type' => 'select',
      '#options' => drupal_map_assoc(range(-50, 50)),
      '#default_value' => $settings->settings['weight'],
      '#attributes' => array('class' => array('facetapi-facet-weight')),
    );

    $options[$facet_name] = '';
    $default_value[$facet_name] = empty($settings->enabled) ? 0: $facet_name;
  }

  // Sorts by the weight appended above.
  uasort($form['#facetapi']['facet_info'], 'drupal_sort_weight');

  $form['table']['enabled_facets'] = array(
    '#type' => 'checkboxes',
    '#options' => $options,
    '#default_value' => $default_value,
  );

  $form['actions'] = array(
    '#type' => 'actions',
    '#weight' => 20,
  );

  $form['actions']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Save configuration'),
  );

  $form['#submit'][] = 'facetapi_ui_realm_settings_form_submit';

  return $form;
}

/**
 *
 */
function facetapi_ui_realm_settings_form_submit($form, &$form_state) {

  // Pulls variables for code readability.
  $adapter = $form['#facetapi']['adapter'];
  $realm = $form['#facetapi']['realm'];
  $facet_info = $form['#facetapi']['facet_info'];

  // Builds settings object, saves to database.
  foreach ($facet_info as $facet_name => $facet) {
    $weight = $form_state['values']['weight:' . $facet_name];
    $settings = $adapter->getFacet($facet)->getSettings($realm);
    $settings->enabled = empty($form_state['values']['enabled_facets'][$facet_name]) ? 0 : 1;
    $settings->settings['weight'] = $weight;
    if (FALSE === ctools_export_crud_save('facetapi', $settings)) {
      drupal_set_message(t('Error saving settings.'), 'error');
    }
  }
}

/**
 * Themes a realm settings table.
 */
function theme_facetapi_ui_realm_settings_table($variables) {
  $output = '';

  // Gets variales for code readability.
  $searcher = $variables['element']['#facetapi']['adapter']->getSearcher();
  $realm_name = $variables['element']['#facetapi']['realm']['name'];

  $header = array(
    'enabled' => array(),
    'label' => array('data' => t('Facet')),
    'operations' => array('data' => t('Operations'), 'colspan' => 2),
    'weight' => array('data' => t('Weight')),
  );

  // Builds field options.
  $rows = array();
  $facet_info = $variables['element']['#facetapi']['facet_info'];
  foreach ($facet_info as $facet_name => $facet) {
    $rows[$facet_name] = array(
      'class' => array('draggable'),
      'data' => array(
        'indexed' => array(
          'data' => drupal_render($variables['element']['enabled_facets'][$facet_name]),
        ),
        'label' => array(
          'data' => check_plain($facet['label']) . "<div class='description'>" . filter_xss($facet['description']) . '</div>',
        ),
        'operations-1' => array(
          'data' => array(
            '#type' => 'link',
            '#title' => t('Edit'),
            '#href' => facetapi_ui_get_settings_path($searcher, $realm_name, $facet_name, 'edit'),
          ),
        ),
        'operations-2' => array(
          'data' => array(
            '#type' => 'link',
            '#title' => t('Export'),
            '#href' => facetapi_ui_get_settings_path($searcher, $realm_name, $facet_name, 'export'),
          ),
        ),
        'weight' => array(
          'data' => drupal_render($variables['element']['weight:' . $facet_name]),
        ),
      ),
    );
  }

  drupal_add_tabledrag('facetapi-ui-realm-settings', 'order', 'sibling', 'facetapi-facet-weight');
  $output .= drupal_render_children($variables['element']);
  $output .= theme('table', array('header' => $header, 'rows' => $rows, 'attributes' => array('id' => 'facetapi-ui-realm-settings')));

  return $output;
}

/**
 * Returns path to search page.
 */
function facetapi_ui_get_settings_path($searcher, $realm_name, $facet_name, $op) {
  $path_parts = array('admin', 'config', 'search', 'facetapi');
  $path_parts[] = $searcher;
  $path_parts[] = $realm_name;
  $path_parts[] = $facet_name;
  $path_parts[] = $op;
  return join('/', $path_parts);
}

/**
 *
 */
function facetapi_ui_facet_settings_form($form, &$form_state, FacetapiAdapter $adapter, array $realm, array $facet) {

  $searcher = $adapter->getSearcher();
  $settings = $adapter->getFacet($facet)->getSettings($realm);

  // Adds LuceneAPI settings, excluded values aren't saved.
  $form['#facetapi'] = array(
    'adapter' => $adapter,
    'realm' => $realm,
    'facet' => $facet,
    'sort_info' => array(),
    'excluded_values' => array(
      'form_build_id', 'form_token', 'form_id', 'op', 'submit', 'submit_list',
      'settings__active_tab',
    ),
  );

  // Orders the sorts by weight.
  foreach (facetapi_get_sort_info() as $sort_name => $sort_info) {
    // @todo Figure out a better default system.
    if (isset($settings->settings['sort_weight'][$sort_name])) {
      $weight = $settings->settings['sort_weight'][$sort_name];
    }
    else {
      $weight = 0;
    }
    $form['#facetapi']['sort_info'][$sort_name] = $sort_info;
    $form['#facetapi']['sort_info'][$sort_name]['weight'] = $weight;
  }
  uasort($form['#facetapi']['sort_info'], 'drupal_sort_weight');

  $form['global'] = array(
    '#type' => 'fieldset',
    '#title' => t('Global settings'),
    '#description' => t(
      'Configuration settings for the %facet facet across <em>all</em> realms that apply to %searcher searches.',
      array(
        '%facet' => $facet['label'],
        '%searcher' => $searcher,
      )
    ),
    '#collapsible' => TRUE,
  );

  $form['global']['operator'] = array(
    '#type' => 'radios',
    '#title' => t('Operator'),
    '#default_value' => $settings->settings['operator'],
    '#options' => array(
      FACETAPI_OPERATOR_AND => t('AND'),
      FACETAPI_OPERATOR_OR => t('OR'),
    ),
    '#description' => t('AND filters are exclusive. OR filters are inclusive. Selecting more AND filters narrows the result set. Selecting more OR filters widens the result set.')
  );

  $form['global']['hard_limit'] = array(
    '#type' => 'select',
    '#title' => t('Hard limit'),
    '#default_value' => $settings->settings['hard_limit'],
    '#options' => drupal_map_assoc(array(100, 75, 50, 40, 30, 20, 15, 10, 5, 3)),
    '#description' => t('Display no more than this number of facet items.')
  );

  $form['realm'] = array(
    '#type' => 'fieldset',
    '#title' => t('@realm realm settings', array('@realm' => $realm['label'])),
    '#description' => t(
      'Configuration settings for the %facet facet in the %realm realm that apply to %searcher searches.',
      array(
        '%facet' => $facet['label'],
        '%realm' => $realm['label'],
        '%searcher' => $adapter->getSearcher(),
      )
    ),
    '#collapsible' => TRUE,
  );

  // Gets widgets, sorts.
  /*
  $widget_info = facetapi_get_widget_info(array(
    'realm' => $realm,
    'facet' => $facet,
  ));
  uasort($widget_info, 'drupal_sort_weight');
  */

  // Builds select options for widgets.
  ctools_include('plugins');
  $widget_options = array();
  foreach (ctools_get_plugins('facetapi', 'widgets') as $id => $plugin) {
    $widget_options[$id] = $plugin['handler']['label'];
  }

  $form['realm']['widget'] = array(
    '#type' => 'select',
    '#title' => t('Display widget'),
    '#default_value' => $settings->settings['widget'],
    '#options' => $widget_options,
  );

  $form['realm']['soft_limit'] = array(
    '#type' => 'select',
    '#title' => t('Soft limit'),
    '#default_value' => $settings->settings['soft_limit'],
    '#options' => array(0 => t('No limit')) + drupal_map_assoc(array(50, 40, 30, 20, 15, 10, 5, 3)),
    '#description' => t('Limits the number of displayed facets via JavaScript.'),
  );

  // Initializes the sorting table.
  $form['realm']['sort'] = array(
    '#type' => 'fieldset',
    '#title' => t('Sorting options'),
    '#description' => t('Determine how facet items are sorted. Selecting the checkbox activates the associated sort, and the sorts are applied from the top down. Click and drag the rows to change the order the sorts are applied.'),
  );

  $form['realm']['sort']['table'] = array(
    '#theme' => 'facetapi_ui_sort_settings_table',
    '#facetapi' => &$form['#facetapi'],
  );

  // Builds checkbox options for sorts.
  $sort_options = array();
  foreach ($form['#facetapi']['sort_info'] as $sort_name => $sort) {
    $sort_options[$sort_name] = '';

    // @todo Figure out a better default system.
    if (isset($settings->settings['sort_order'][$sort_name])) {
      $default_value = $settings->settings['sort_order'][$sort_name];
    }
    else {
      $default_value = SORT_ASC;
    }
    $form['realm']['sort']['table']['sort_order:' . $sort_name] = array(
      '#type' => 'select',
      '#title' => '',
      '#options' => array(
        SORT_DESC => t('Descending'),
        SORT_ASC => t('Ascending'),
      ),
      '#default_value' => $default_value,
    );

    // @todo Figure out a better default system.
    if (isset($settings->settings['sort_weight'][$sort_name])) {
      $default_value = $settings->settings['sort_weight'][$sort_name];
    }
    else {
      $default_value = 0;
    }
    $form['realm']['sort']['table']['sort_weight:'. $sort_name] = array(
      '#type' => 'weight',
      '#title' => '',
      '#delta' => 50,
      '#default_value' => $default_value,
      '#attributes' => array('class' => array('facetapi-sort-weight')),
    );
  }

  $form['realm']['sort']['table']['active_sorts'] = array(
    '#type' => 'checkboxes',
    '#options' => $sort_options,
    '#default_value' => $settings->settings['active_sorts'],
  );

  $form['actions'] = array(
    '#type' => 'actions',
    '#weight' => 20,
  );

  $form['actions']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Save configuration'),
  );

  $searcher_info = facetapi_get_searcher_info();
  $form['actions']['cancel'] = array(
    '#type' => 'link',
    '#title' => t('Cancel'),
    '#href' => $searcher_info[$searcher]['path'] . '/facets/' . $realm['name'],
  );

  $form['#submit'][] = 'facetapi_ui_facet_settings_form_submit';

  return $form;
}

/**
 * Themes a sort settings table.
 */
function theme_facetapi_ui_sort_settings_table($variables) {
  $output = '';

  // Gets variales for code readability.
  $searcher = $variables['element']['#facetapi']['adapter']->getSearcher();

  $header = array(
    'enabled' => array(),
    'label' => array('data' => t('Sort')),
    'order' => array('data' => t('Order')),
    'weight' => array('data' => t('Weight')),
  );

  // Builds field options.
  $rows = array();
  $sort_info = $variables['element']['#facetapi']['sort_info'];
  foreach ($sort_info as $sort_name => $sort) {
    $rows[$sort_name] = array(
      'class' => array('draggable'),
      'data' => array(
        'enabled' => array(
          'data' => drupal_render($variables['element']['active_sorts'][$sort_name]),
        ),
        'label' => array(
          'data' => check_plain($sort['label']) . "<div class='description'>" . filter_xss($sort['description']) . '</div>',
        ),
        'order' => array(
          'data' => drupal_render($variables['element']['sort_order:' . $sort_name]),
        ),
        'weight' => array(
          'data' => drupal_render($variables['element']['sort_weight:' . $sort_name]),
        ),
      ),
    );
  }

  drupal_add_tabledrag('facetapi-ui-sort-settings', 'order', 'sibling', 'facetapi-sort-weight');
  $output .= drupal_render_children($variables['element']);
  $output .= theme('table', array('header' => $header, 'rows' => $rows, 'attributes' => array('id' => 'facetapi-ui-sort-settings')));

  return $output;
}

/**
 * Processes facetapi_ui_facet_settings_form submissions.
 */
function facetapi_ui_facet_settings_form_submit($form, &$form_state) {

  // Pulls variables for code readability.
  $adapter = $form['#facetapi']['adapter'];
  $realm = $form['#facetapi']['realm'];
  $facet = $form['#facetapi']['facet'];

  // Loads settings, saves all form values as settings other than excluded.
  $settings = $adapter->getFacet($facet)->getSettings($realm);
  $settings->settings = array_merge($settings->settings, array_diff_key(
    $form_state['values'],
    array_flip($form['#facetapi']['excluded_values'])
  ));

  // Converts sort_weight:* and sort_order:* settings to associative arrays.
  facetapi_ui_combine_settings('sort_weight', $settings);
  facetapi_ui_combine_settings('sort_order', $settings);

  if (FALSE === ctools_export_crud_save('facetapi', $settings)) {
    drupal_set_message(t('Error saving settings.'), 'error');
  }
}

/**
 * Combines settings named "setting:*" into a single array of settings.
 *
 * This makes it easier to process settings at runtime.
 *
 * @param $setting_name
 *   A string containing the name of the setting.
 * @param $settings
 *   An object containing the settings loaded from CTools.
 */
function facetapi_ui_combine_settings($setting_name, $settings) {
  $start = strlen($setting_name) + 1;
  $settings->settings[$setting_name] = array();
  foreach ($settings->settings as $name => $value) {
    if (0 === strpos($name, $setting_name . ':')) {
      $key = substr($name, $start);
      unset($settings->settings[$name]);
      $settings->settings[$setting_name][$key] = $value;
    }
  }
}