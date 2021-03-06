/*
 * Copyright 2019 Google
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

import CommandLineKit
import Foundation
import MetricsLib

var flags = Flags()
let coveragePath = flags.string("c",
                                "coverage",
                                description: "Required - The path of the JSON coverage report generated by XCov.")
let outputPath = flags.string("o",
                              "output",
                              description: "Required - The path to write the database JSON info.")
let pullRequest = flags.int("p",
                            "pull_request",
                            description: "Required - The number of the pull request that corresponds to this coverage run.")
do {
  try flags.parse()
  if !coveragePath.wasSet {
    print("Please specify the path of the JSON coverage report from XCov. -c or --coverage")
    exit(1)
  }
  if !outputPath.wasSet {
    print("Please specify output location for the database JSON file. -o or --output")
    exit(1)
  }
  if !pullRequest.wasSet {
    print("Please specify the corresponding pull request number. -p or --pull_request")
    exit(1)
  }
  let pullRequestTable = TableUpdate(table_name: "PullRequests",
                                     column_names: ["pull_request_id"],
                                     replace_measurements: [[Double(pullRequest.value!)]])
  let coverageReport = try CoverageReport.load(path: coveragePath.value!)
  let coverageTable = TableUpdate.createFrom(coverage: coverageReport,
                                             pullRequest: pullRequest.value!)
  let json = try UploadMetrics(tables: [pullRequestTable, coverageTable]).json()
  try json.write(to: NSURL(fileURLWithPath: outputPath.value!) as URL,
                 atomically: false,
                 encoding: .utf8)
  print("Successfully created \(outputPath.value!)")
} catch {
  print("Error occurred: \(error)")
  exit(1)
}
