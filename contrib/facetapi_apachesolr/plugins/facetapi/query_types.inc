<?php

/**
 * @file
 * Query type plugins for the Apache Solr module.
 */

/**
 * Plugin for "term" query types.
 */
class FacetapiApachesolrTerm extends FacetapiQueryType {

  /**
   * Returns the query type associated with the plugin.
   *
   * @return string
   *   The query type.
   */
  static public function getType() {
    return 'term';
  }

  /**
   * Adds the filter to the query object.
   *
   * @param stdClass $query
   *   An object containing the query in the backend's native API.
   */
  public function execute($query) {
    $searcher = $this->adapter->getSearcher();

    // Adds the operator parameter.
    //$operator = facetapi_get_setting('operator', $searcher, '', $this->facet['name']);
    $operator = 'and';
    $ex = (FACETAPI_OPERATOR_OR != $operator) ? '' : "{!ex={$this->facet['field']}}";
    $query->params['facet.field'][] = $ex . $this->facet['field'];

    // Adds "hard limit" parameter to prevent too many return values.
    //$limit = facetapi_get_setting('hard_limit', $searcher, '', $this->facet['name']);
    $limit = 50;
    $query->params['f.' . $this->facet['field'] . '.facet.limit'] = ($limit !== NULL) ? (int) $limit : 20;
  }

  /**
   * Initializes the facet's build array.
   *
   * @return array
   *   The initialized render array.
   */
  public function build() {
    $build = array();
    if ($response = apachesolr_static_response_cache()) {
      $values = (array) $response->facet_counts->facet_fields->{$this->facet['field']};
      foreach ($values as $value => $count) {
        $build[$value] = array('#count' => $count);
      }
    }
    return $build;
  }
}

/**
 * Plugin for "date" query types.
 */
class FacetapiApachesolrDate extends FacetapiQueryType {

  /**
   * Returns the query type associated with the plugin.
   *
   * @return string
   *   The query type.
   */
  static public function getType() {
    return 'date';
  }

  /**
   * Adds the filter to the query object.
   *
   * @param stdClass $query
   *   An object containing the query in the backend's native API.
   */
  public function execute($query) {
    $searcher = $this->adapter->getSearcher();

    // Gets the data range in formats that Solr understands.
    list($start, $end, $gap) = $this->getDateRange($query);

    $query->params['facet.date'][] = $this->facet['field'];
    $query->params['f.' . $this->facet['field'] . '.facet.date.start'] = $start;
    $query->params['f.' . $this->facet['field'] . '.facet.date.end'] = $end;
    $query->params['f.' . $this->facet['field'] . '.facet.date.gap'] = $gap;

    // Adds "hard limit" parameter to prevent too many return values.
    //$limit = facetapi_setting_get('hard_limit', $searcher, '', $this->facet['name']);
    $limit = 20;
    $query->params['f.' . $this->facet['field'] . '.facet.limit'] = ($limit !== NULL) ? (int) $limit : 20;
  }

  /**
   * Initializes the facet's build array.
   *
   * @return array
   *   The initialized render array.
   */
  public function build() {

    // Initializes build and gets static response.
    if (!$response = apachesolr_static_response_cache()) {
      return array();
    }
    $build = array();

    // Gets total number of documents matched in search.
    $total = $response->response->numFound;

    // Gets the active date facets, starts to builds the "parent - child"
    // relationships.
    $parent = NULL;
    foreach ($this->adapter->getActiveItems($this->facet) as $value => $item) {

      // Builds the raw facet "value", the count for selected items will be the
      // total number of rows returned in the query.
      $build[$value] = array('#count' => $total);

      // If there is a previous item, there is a parent, uses a reference so the
      // arrays are populated when they are updated.
      if (NULL !== $parent) {
        $build[$parent]['#item_children'][$value] = &$build[$value];
        $build[$value]['#item_parents'][$parent] = $parent;
      }

      // Stores the last value iterated over.
      $parent = $value;
    }

    // Gets raw facet data from the Solr server.
    if (isset($response->facet_counts->facet_dates)) {
      $raw_data = (array) $response->facet_counts->facet_dates->{$this->facet['field']};
    }
    else {
      $raw_data = array();
    }

    $end = (!empty($raw_data['end'])) ? $raw_data['end'] : '';
    $gap = (!empty($raw_data['gap'])) ? $raw_data['gap'] : '';

    unset($raw_data['end']);
    unset($raw_data['gap']);

    // Treat each date facet as a range start, and use the next date facet
    // as range end.  Use 'end' for the final end.
    $range_end = array();
    $previous = NULL;
    foreach ($raw_data as $value => $count) {
      if (isset($previous)) {
        $range_end[$previous] = $value;
      }
      $previous = $value;
    }
    $range_end[$previous] = $end;

    // Builds facet counts object used by the server.
    foreach ($raw_data as $value => $count) {
      if ($count) {
        $new_value = '['. $value .' TO '. $range_end[$value] .']';
        $build[$new_value] = array('#count' => $count, '#active' => 0);
        if (NULL !== $parent) {
          $build[$parent]['#item_children'][$new_value] = &$build[$new_value];
          $build[$new_value]['#item_parents'][$parent] = $parent;
        }
      }
    }

    return $build;
  }

  /**
   * Gets the range of dates we are using.
   *
   * @param $query
   *   A SolrBaseQuery object.
   *
   * @return
   *   An array containing the gap and range information.
   */
  function getDateRange(SolrBaseQuery $query) {

    // Attempts to get next gap from passed date filters.
    $return = NULL;
    foreach ($this->adapter->getActiveItems($this->facet) as $value => $item) {
      if ($gap = facetapi_date_gap_get($item['start'], $item['end'])) {
        $next_gap = facetapi_next_date_gap_get($gap, FACETAPI_DATE_MINUTE);
        if ($next_gap == $gap) {
          $next_gap = NULL;
        }
        $return = array(
          "{$item['start']}/$next_gap",
          "{$item['end']}+1$next_gap/$next_gap",
          "+1$next_gap",
        );
      }
    }

    // If no filters were passed, get default range.
    if (NULL === $return) {

      // Builds SQL that gets minimum and maximum values from node table.
      $minimum = $maximum = FALSE;
      if (!empty($this->facet['min callback']) && function_exists($this->facet['min callback'])) {
        $minimum = $this->facet['min callback']($this->facet);
      }
      if (!empty($this->facet['max callback']) && function_exists($this->facet['max callback'])) {
        $maximum = $this->facet['max callback']($this->facet);
      }

      // Gets the default gap.
      $gap = FACETAPI_DATE_YEAR;
      if ($minimum && $maximum) {
        $gap = facetapi_timestamp_gap_get($minimum, $maximum);
        $minimum = facetapi_isodate($minimum, $gap);
        $maximum = facetapi_isodate($maximum, $gap);
        $return = array(
          "$minimum/$gap",
          "$maximum+1$gap/$gap",
          "+1$gap",
        );
      }
    }

    // Returns the range information.
    return $return;
  }
}