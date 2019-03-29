from git import Repo
from SingleTest import SingleTest

dMPI = [True, False]
dOpenMP = [True, False]
dVec = ["AVX", "AVX2", "SSE"]
dRMM = [True, False]
dSize = [50]


class Commit:
    """Commit

    Running a full or partial test suite for a given commit
    """

    def __init__(self, repo, commit):
        # Reset Dir to specified commit
        repo.head.reset(commit, index=True, working_tree=True)
        print("New commit:", commit)
        self.commit = commit
        self.repo = repo

    def singleDimension(self, db, config):
        """Run tests for a commit only on a single config"""
        print("Single config:", self.commit, config)
        t = SingleTest(path=self.repo.git_dir, commit=self.commit, mpi=config["mpi"], openMP=config["openMP"], vec=config["vec"], RMM=config["RMM"],
                       size=config["size"])
        t.test()
        t.save(db)

    def partialRun(self):
        """Partial run for test configurations. Test Dimensions..."""
        print("partial run")

    def fullRun(self, db):
        """Run through the entire configuration space for a given commit."""
        print("Full Run ", self.commit)
        for mpi in dMPI:
            for openMP in dOpenMP:
                for vec in dVec:
                    for RMM in dRMM:
                        for size in dSize:
                            t = SingleTest(path=self.repo.git_dir, commit=self.commit, mpi=mpi, openMP=openMP, vec=vec, RMM=RMM, size=size)
                            t.test()
                            t.save(db)
