def get-ecsact-deps [] {
	(buildozer -output_json 'print name version' //MODULE.bazel:%bazel_dep
		| from json
		| get records
		| each {|record| {name: $record.fields.0.text, version: $record.fields.1.text?} }
		| filter {|dep| $dep.name | str starts-with "ecsact_"}
	)
}

def main [version, --dry-run, --update] {
	let start_dir = $env.PWD;
	let tmp_repo_dir = mktemp -d --suffix "EcsactSdkRelease";
	let last_release = (gh release view --json tagName | from json).tagName;
	git worktree add $tmp_repo_dir $last_release;
	cd $tmp_repo_dir;
	let before_update_deps = get-ecsact-deps;
	git worktree remove $tmp_repo_dir --force | complete; # this sometimes fails
	cd $start_dir;
	let changelog_template = [$start_dir, "release-notes-template"] | path join;
	
	if $update {
		$before_update_deps | each {|dep| bzlmod add $dep.name; };
	}
	
	# sanity check
	bazel build "//...";

	let release_notes = (get-ecsact-deps | each {|dep| 
		print $"Generating release notes for ($dep.name)";
		let before_version = ($before_update_deps | where name == $dep.name);
		if ($before_version | length) == 0 {
			return $"## NEW dependency ($dep.name)\n\n";
		}
		let before_version = $before_version | get version | get 0;
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

	if $dry_run {
		echo $release_notes;
	} else {
		if $update {
			git add MODULE.bazel;
			git commit -m $"chore\(deps\): ecsact repos for ($version) release";
			git push origin main;
		}
		git tag $version;
		git push origin $version;
		gh release create $version -n $release_notes --latest -t $version --verify-tag --latest;
	}
}
