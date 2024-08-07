def get-ecsact-deps [] {
	(buildozer -output_json 'print name version' //MODULE.bazel:%bazel_dep
		| from json
		| get records
		| each {|record| {name: $record.fields.0.text, version: $record.fields.1.text?} }
		| filter {|dep| $dep.name | str starts-with "ecsact_"}
	)
}

def main [version: string] {
	let start_dir = $env.PWD;
	let before_update_deps = get-ecsact-deps;
	let changelog_template = [$start_dir, "release-notes-template"] | path join;

	$before_update_deps | each {|dep| bzlmod add $dep.name; };

	let release_notes = (get-ecsact-deps | each {|dep| 
		let before_version = $before_update_deps | where name == $dep.name | get version | get 0;
		let after_version = $dep | get version;
		let dep_repo_remote = $"https://github.com/ecsact-dev/($dep.name)";
		let cached_repo_dir = $".cache/repos/($dep.name)";
		mut dep_release_notes = "";

		if $after_version != $before_version {
			rm -rf $cached_repo_dir;
			git clone --quiet $dep_repo_remote $cached_repo_dir;
			cd $cached_repo_dir;
			let changelog = cog changelog $"($before_version)..($after_version)" -o ecsact-dev --repository $dep.name --remote "github.com" -t $changelog_template;
			rm -rf $cached_repo_dir;
			cd $start_dir;
			$dep_release_notes += $"# ($dep.name) ($before_version) -> ($after_version)\n\n";
			$dep_release_notes += $changelog;
		}

		$dep_release_notes
	});

	let release_notes = $release_notes | reduce {|$section, $full| $full + $section} -f "";

	git commit -m $"chore(deps): ecsact repos for ($version) release";
	git push origin main;
	git tag $version;
	git push origin $version;
	gh release create $version -n $release_notes --latest -t $version --verify-tag --latest;
}
